//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Unit tests for gethostbyname_r and gethostbyname.
///
//===----------------------------------------------------------------------===//

#include "hdr/errno_macros.h"
#include "hdr/netdb_macros.h"
#include "hdr/poll_macros.h"
#include "hdr/signal_macros.h"
#include "hdr/stdint_proxy.h"
#include "hdr/sys_socket_macros.h"
#include "hdr/types/size_t.h"
#include "hdr/types/socklen_t.h"
#include "hdr/types/ssize_t.h"
#include "hdr/types/struct_hostent.h"
#include "hdr/types/struct_in_addr.h"
#include "hdr/types/struct_sockaddr.h"
#include "hdr/types/struct_sockaddr_in.h"
#include "src/__support/CPP/array.h"
#include "src/__support/CPP/scope.h"
#include "src/__support/CPP/span.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/OSUtil/network.h"
#include "src/__support/net/byte_order.h"
#include "src/__support/netdb/dns_client.h"
#include "src/__support/netdb/dns_wire.h"
#include "src/__support/netdb/h_errno.h"
#include "src/__support/netdb/hosts.h"
#include "src/netdb/gethostbyname.h"
#include "src/netdb/gethostbyname_r.h"
#include "src/string/memory_utils/inline_memcpy.h"
#include "src/string/string_utils.h"
#include "src/sys/socket/bind.h"
#include "src/sys/socket/getsockname.h"
#include "src/sys/socket/recvfrom.h"
#include "src/sys/socket/sendto.h"
#include "src/sys/socket/socket.h"
#include "src/unistd/close.h"
#include "test/UnitTest/ErrnoCheckingTest.h"
#include "test/UnitTest/Test.h"
#include "test/src/netdb/test_db_file.h"

namespace {

constexpr void make_alias_name(char (&out)[16], size_t index) {
  constexpr LIBC_NAMESPACE::cpp::string_view PREFIX = "host_alias_";
  for (size_t i = 0; i < PREFIX.size(); ++i)
    out[i] = PREFIX[i];
  out[PREFIX.size()] = static_cast<char>('0' + (index / 10));
  out[PREFIX.size() + 1] = static_cast<char>('0' + (index % 10));
  out[PREFIX.size() + 2] = '\0';
}

} // namespace

class LlvmLibcGethostbynameTest
    : public LIBC_NAMESPACE::testing::ErrnoCheckingTest {
protected:
  void TearDown() override {
    LIBC_NAMESPACE::netdb::set_dns_wire_hook(nullptr);
    LIBC_NAMESPACE::netdb::set_dns_port_override(0);
    LIBC_NAMESPACE::netdb::set_hosts_path(nullptr);
    ErrnoCheckingTest::TearDown();
  }
};

TEST_F(LlvmLibcGethostbynameTest, LiteralHostsTwentyAliasesAndBounds) {
  using LIBC_NAMESPACE::cpp::span;
  using LIBC_NAMESPACE::cpp::string_view;

  LIBC_NAMESPACE::netdb::set_dns_wire_hook(
      [](const LIBC_NAMESPACE::netdb::ScopedIpAddr &, bool,
         span<const uint8_t> q, span<uint8_t> r) -> int {
        using namespace LIBC_NAMESPACE::netdb;
        LIBC_NAMESPACE::inline_memcpy(r.data(), q.data(), q.size());
        write_u16_be(r, 2, DNS_FLAG_QR | 3);
        return static_cast<int>(q.size());
      });

  struct hostent he{};
  struct hostent *res = &he;
  alignas(char *) char buf[1024];
  int herr = 0;

  // Exercise default nullptr hosts_path with RFC 6761 localhost synthesis.
  ASSERT_EQ(LIBC_NAMESPACE::gethostbyname_r("hermetic-probe.localhost", &he,
                                            buf, sizeof(buf), &res, &herr),
            0);
  ASSERT_EQ(res, &he);
  EXPECT_STREQ(he.h_name, "hermetic-probe.localhost");
  EXPECT_EQ(static_cast<uint8_t>(he.h_addr[0]), uint8_t(127));
  EXPECT_EQ(static_cast<uint8_t>(he.h_addr[3]), uint8_t(1));

  // Missing /etc/hosts file returns 0 + HOST_NOT_FOUND cleanly.
  LIBC_NAMESPACE::netdb::set_hosts_path(
      libc_make_test_file_path("host_r_missing.test"));
  EXPECT_EQ(LIBC_NAMESPACE::gethostbyname_r("missing.local", &he, buf,
                                            sizeof(buf), &res, &herr),
            0);
  EXPECT_EQ(res, nullptr);
  EXPECT_EQ(herr, HOST_NOT_FOUND);

  constexpr size_t TWENTY_ALIASES = 20;
  char db[2400];
  size_t pos = 0;
  auto append = [&](string_view s) {
    for (char ch : s)
      db[pos++] = ch;
  };

  // Non-matching >512B line before matches, IPv6-only record, multi-homed IPv4.
  append("10.0.0.1 ");
  for (size_t i = 0; i < 40; ++i)
    append("other_padding ");
  append("other.local\n");
  append("::1 v6.local\n");
  append("::1 app.local app\n");
  append("10.9.8.7 app.local app a1\n");
  append("10.9.8.8 app.local app\n");

  // 20-alias >512B /etc/hosts line.
  append("10.2.0.1 wide20.local");
  for (size_t i = 0; i < TWENTY_ALIASES; ++i) {
    char alias[16];
    make_alias_name(alias, i);
    db[pos++] = ' ';
    append(string_view(alias, 13));
  }
  for (size_t i = 0; i < 20; ++i)
    append("        ");
  db[pos++] = '\n';

  // >16 multi-homed IPv4 addresses cap.
  for (size_t i = 0; i < 18; ++i)
    append("10.3.0.1 multi18.local\n");

  ScopedTestDbFile hdb(libc_make_test_file_path("host_r.test"),
                       string_view(db, pos),
                       &LIBC_NAMESPACE::netdb::set_hosts_path);

  // Dotted-quad IPv4 literal with nullable h_errnop and ERANGE bounds.
  ASSERT_EQ(LIBC_NAMESPACE::gethostbyname_r("192.0.2.55", &he, buf, sizeof(buf),
                                            &res, nullptr),
            0);
  ASSERT_EQ(res, &he);
  EXPECT_STREQ(he.h_name, "192.0.2.55");
  EXPECT_EQ(he.h_addrtype, AF_INET);
  EXPECT_EQ(he.h_length, 4);
  EXPECT_EQ(he.h_aliases[0], nullptr);
  EXPECT_EQ(static_cast<uint8_t>(he.h_addr[3]), uint8_t(55));
  EXPECT_EQ(he.h_addr_list[1], nullptr);

  herr = 0;
  EXPECT_EQ(LIBC_NAMESPACE::gethostbyname_r("192.0.2.55", &he, nullptr, 0, &res,
                                            &herr),
            ERANGE);
  EXPECT_EQ(res, nullptr);
  EXPECT_EQ(herr, NETDB_INTERNAL);

  herr = 0;
  EXPECT_EQ(
      LIBC_NAMESPACE::gethostbyname_r("192.0.2.55", &he, buf, 16, &res, &herr),
      ERANGE);
  EXPECT_EQ(herr, NETDB_INTERNAL);

  // Multi-homed /etc/hosts match preserves first-line aliases and both IPs.
  ASSERT_EQ(LIBC_NAMESPACE::gethostbyname_r("APP", &he, buf, sizeof(buf), &res,
                                            &herr),
            0);
  ASSERT_EQ(res, &he);
  EXPECT_STREQ(he.h_name, "app.local");
  EXPECT_STREQ(he.h_aliases[0], "app");
  EXPECT_STREQ(he.h_aliases[1], "a1");
  EXPECT_EQ(he.h_aliases[2], nullptr);
  EXPECT_EQ(static_cast<uint8_t>(he.h_addr_list[0][3]), uint8_t(7));
  ASSERT_NE(he.h_addr_list[1], nullptr);
  EXPECT_EQ(static_cast<uint8_t>(he.h_addr_list[1][3]), uint8_t(8));
  EXPECT_EQ(he.h_addr_list[2], nullptr);

  // Sweep every buffer capacity from 0..80 to exercise text, padding, ptrs,
  // and address ERANGE boundaries without modifying errno.
  EXPECT_EQ(
      LIBC_NAMESPACE::gethostbyname_r("app", &he, nullptr, 0, &res, nullptr),
      ERANGE);
  for (size_t cap = 1; cap < 80; ++cap) {
    herr = 0;
    const int RC =
        LIBC_NAMESPACE::gethostbyname_r("app", &he, buf, cap, &res, &herr);
    if (RC != 0) {
      EXPECT_EQ(RC, ERANGE);
      EXPECT_EQ(res, nullptr);
      EXPECT_EQ(herr, NETDB_INTERNAL);
    }
  }

  // 20-alias >512B record resolves last alias and returns all 20 aliases.
  ASSERT_EQ(LIBC_NAMESPACE::gethostbyname_r("HOST_ALIAS_19", &he, buf,
                                            sizeof(buf), &res, &herr),
            0);
  ASSERT_EQ(res, &he);
  EXPECT_STREQ(he.h_name, "wide20.local");
  for (size_t i = 0; i < TWENTY_ALIASES; ++i) {
    char expected[16];
    make_alias_name(expected, i);
    ASSERT_NE(he.h_aliases[i], nullptr);
    EXPECT_STREQ(he.h_aliases[i], expected);
  }
  EXPECT_EQ(he.h_aliases[TWENTY_ALIASES], nullptr);

  ASSERT_EQ(LIBC_NAMESPACE::gethostbyname_r("multi18.local", &he, buf,
                                            sizeof(buf), &res, &herr),
            0);
  ASSERT_EQ(res, &he);
  EXPECT_EQ(he.h_addr_list[16], nullptr);

  ASSERT_EQ(LIBC_NAMESPACE::gethostbyname_r("api.localhost.", &he, buf,
                                            sizeof(buf), &res, &herr),
            0);
  EXPECT_STREQ(he.h_name, "api.localhost");

  struct hostent *tls = LIBC_NAMESPACE::gethostbyname("host_alias_19");
  ASSERT_NE(tls, nullptr);
  EXPECT_STREQ(tls->h_name, "wide20.local");
  EXPECT_STREQ(tls->h_aliases[19], "host_alias_19");
  EXPECT_EQ(tls->h_aliases[20], nullptr);

  tls = LIBC_NAMESPACE::gethostbyname("api.localhost.");
  ASSERT_NE(tls, nullptr);
  EXPECT_STREQ(tls->h_name, "api.localhost");
  EXPECT_EQ(static_cast<uint8_t>(tls->h_addr[0]), uint8_t(127));

  EXPECT_EQ(LIBC_NAMESPACE::gethostbyname("192.168.1.999"), nullptr);
  EXPECT_EQ(*LIBC_NAMESPACE::netdb::h_errno_addr(), HOST_NOT_FOUND);

  static constexpr const char *MISS_CASES[] = {
      "bad.invalid",
      "v6.local",
      "1.2.3.999",
  };
  for (const char *miss : MISS_CASES) {
    herr = 0;
    EXPECT_EQ(LIBC_NAMESPACE::gethostbyname_r(miss, &he, buf, sizeof(buf), &res,
                                              &herr),
              0);
    EXPECT_EQ(res, nullptr);
    EXPECT_EQ(herr, HOST_NOT_FOUND);
  }
  EXPECT_EQ(LIBC_NAMESPACE::gethostbyname_r("v6.local", &he, buf, sizeof(buf),
                                            &res, nullptr),
            0);

  // Open failure (ENOTDIR) and read failure (EISDIR) set NO_RECOVERY.
  char notdir_path[280];
  const size_t BASE_LEN = LIBC_NAMESPACE::internal::strlcpy(
      notdir_path, hdb.get_path(), sizeof(notdir_path));
  LIBC_NAMESPACE::internal::strlcpy(notdir_path + BASE_LEN, "/hosts",
                                    sizeof(notdir_path) - BASE_LEN);
  LIBC_NAMESPACE::netdb::set_hosts_path(notdir_path);
  herr = 0;
  EXPECT_EQ(LIBC_NAMESPACE::gethostbyname_r("app", &he, buf, sizeof(buf), &res,
                                            &herr),
            ENOTDIR);
  EXPECT_EQ(herr, NO_RECOVERY);

  LIBC_NAMESPACE::netdb::set_hosts_path(".");
  herr = 0;
  EXPECT_EQ(LIBC_NAMESPACE::gethostbyname_r("app", &he, buf, sizeof(buf), &res,
                                            &herr),
            EISDIR);
  EXPECT_EQ(herr, NO_RECOVERY);
}

TEST_F(LlvmLibcGethostbynameTest, DnsWireCodecPointerCompressionAndHostChecks) {
  using namespace LIBC_NAMESPACE::netdb;
  using LIBC_NAMESPACE::cpp::array;
  using LIBC_NAMESPACE::cpp::span;

  ScopedTestDbFile hdb(libc_make_test_file_path("host_wire.test"), "",
                       &set_hosts_path);

  struct hostent he{};
  struct hostent *res = nullptr;
  alignas(char *) char buf[512];
  int herr = 0;

  static constexpr const char *BAD_HOSTS[] = {
      "::1",
      "a..b",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.com",
      "-lead.example",
      "trail-.example",
      "under_score.example",
  };
  for (const char *bad : BAD_HOSTS) {
    herr = 0;
    EXPECT_EQ(LIBC_NAMESPACE::gethostbyname_r(bad, &he, buf, sizeof(buf), &res,
                                              &herr),
              0);
    EXPECT_EQ(res, nullptr);
    EXPECT_EQ(herr, HOST_NOT_FOUND);
  }

  array<uint8_t, 4> tiny_out{};
  EXPECT_FALSE(encode_qname("example.com", tiny_out).has_value());
  array<uint8_t, DNS_MAX_WIRE_NAME> qout{};
  EXPECT_FALSE(encode_qname("a..b", qout).has_value());
  EXPECT_FALSE(
      encode_qname(
          "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.x",
          qout)
          .has_value());

  static int wire_case = 0;
  set_dns_wire_hook([](const ScopedIpAddr &, bool, span<const uint8_t> q,
                       span<uint8_t> r) -> int {
    const int Q = static_cast<int>(q.size());
    LIBC_NAMESPACE::inline_memcpy(r.data(), q.data(), q.size());
    write_u16_be(r, 2, DNS_FLAG_QR);
    write_u16_be(r, 6, 1);

    auto append_tail = [&](span<const uint8_t> tail) {
      LIBC_NAMESPACE::inline_memcpy(r.data() + Q, tail.data(), tail.size());
      return Q + static_cast<int>(tail.size());
    };

    switch (wire_case) {
    case 0:
      return -1;
    case 1:
      return 8;
    case 2:
      r[0] ^= 0xff;
      return Q;
    case 3:
      write_u16_be(r, 2, 0);
      return Q;
    case 4:
      write_u16_be(r, 4, 2);
      return Q;
    case 5:
      return 12;
    case 6:
      return Q - 2;
    case 7:
      write_u16_be(r, static_cast<size_t>(Q - 4), 28);
      return Q;
    case 8:
      write_u16_be(r, static_cast<size_t>(Q - 2), 3);
      return Q;
    case 9:
      r[13] ^= 1;
      return Q;
    case 10: {
      const uint8_t T[1] = {0xc0};
      return append_tail(T);
    }
    case 11: {
      const uint8_t T[2] = {0xc0, 4};
      return append_tail(T);
    }
    case 12: {
      const uint8_t T[2] = {0xc0, static_cast<uint8_t>(Q)};
      return append_tail(T);
    }
    case 13: {
      // >16 pointer hops rejected.
      write_u16_be(r, 6, 2);
      const uint8_t DH[12] = {0xc0, 12, 0, 1, 0, 3, 0, 0, 0, 5, 0, 32};
      LIBC_NAMESPACE::inline_memcpy(r.data() + Q, DH, 12);
      for (int i = 0; i < 16; ++i) {
        const size_t TARGET =
            i == 0 ? 12 : static_cast<size_t>(Q + 12 + 2 * (i - 1));
        write_u16_be(r, static_cast<size_t>(Q + 12 + 2 * i),
                     static_cast<uint16_t>(0xc000u | TARGET));
      }
      write_u16_be(r, static_cast<size_t>(Q + 44),
                   static_cast<uint16_t>(0xc000u | (Q + 42)));
      return Q + 46;
    }
    case 14: {
      const uint8_t T[1] = {0x40};
      return append_tail(T);
    }
    case 15: {
      const uint8_t T[2] = {10, 'a'};
      return append_tail(T);
    }
    case 16: {
      // Decompressed wire name >255 bytes rejected.
      write_u16_be(r, 6, 2);
      const uint8_t DH[12] = {0xc0, 12, 0, 1, 0, 3, 0, 0, 0, 5, 1, 58};
      LIBC_NAMESPACE::inline_memcpy(r.data() + Q, DH, 12);
      size_t off = static_cast<size_t>(Q + 12);
      size_t prev = 0;
      for (int k = 0; k < 5; ++k) {
        const size_t START = off;
        r[off++] = 60;
        for (int j = 0; j < 60; ++j)
          r[off++] = 'a';
        if (k == 0) {
          r[off++] = 0;
        } else {
          write_u16_be(r, off, static_cast<uint16_t>(0xc000u | prev));
          off += 2;
        }
        prev = START;
      }
      write_u16_be(r, off, static_cast<uint16_t>(0xc000u | prev));
      return static_cast<int>(off + 2);
    }
    case 17:
    case 18:
    case 19: {
      const uint8_t CH = wire_case == 17 ? ' ' : (wire_case == 18 ? 0x7f : '.');
      const uint8_t T[3] = {1, CH, 0};
      return append_tail(T);
    }
    case 20: {
      const uint8_t T[5] = {0xc0, 12, 0, 1, 0};
      return append_tail(T);
    }
    case 21: {
      const uint8_t T[12] = {0xc0, 12, 0, 1, 0, 1, 0, 0, 0, 5, 0, 20};
      return append_tail(T);
    }
    case 22: {
      const uint8_t T[15] = {0xc0, 12, 0, 1, 0, 1, 0, 0, 0, 5, 0, 4, 1, 2, 3};
      return append_tail(T);
    }
    case 23: {
      // Valid RR header with RDLEN != 4 on IN A rejected.
      const uint8_t T[15] = {0xc0, 12, 0, 1, 0, 1, 0, 0, 0, 5, 0, 3, 1, 2, 3};
      return append_tail(T);
    }
    case 24: {
      // Valid response with 0 matching A records (non-IN class + non-A type +
      // different owner) -> NXDOMAIN/NODATA miss.
      write_u16_be(r, 6, 3);
      const uint8_t T[] = {
          0xc0, 12, 0, 1,  0, 3, 0, 0, 0, 5, 0, 4, 1,   2, 3,   4,
          0xc0, 12, 0, 16, 0, 1, 0, 0, 0, 5, 0, 1, 'x', 1, 'z', 0,
          0,    1,  0, 1,  0, 0, 0, 5, 0, 4, 1, 2, 3,   4,
      };
      return append_tail(T);
    }
    default: {
      // Valid compressed-owner IN A response (>16 records capped at 16).
      write_u16_be(r, 6, 18);
      size_t off = static_cast<size_t>(Q);
      for (int i = 0; i < 18; ++i) {
        const uint8_t RR[16] = {
            0xc0, 12, 0, 1, 0,   1,  0,   0,
            0,    5,  0, 4, 198, 51, 100, static_cast<uint8_t>(9 + i),
        };
        LIBC_NAMESPACE::inline_memcpy(r.data() + off, RR, sizeof(RR));
        off += sizeof(RR);
      }
      return static_cast<int>(off);
    }
    }
  });

  for (wire_case = 0; wire_case <= 23; ++wire_case) {
    herr = 0;
    EXPECT_EQ(LIBC_NAMESPACE::gethostbyname_r("a.example.", &he, buf,
                                              sizeof(buf), &res, &herr),
              0);
    EXPECT_EQ(res, nullptr);
    EXPECT_EQ(herr, TRY_AGAIN);
  }

  wire_case = 24;
  herr = 0;
  EXPECT_EQ(LIBC_NAMESPACE::gethostbyname_r("a.example.", &he, buf, sizeof(buf),
                                            &res, &herr),
            0);
  EXPECT_EQ(res, nullptr);
  EXPECT_EQ(herr, HOST_NOT_FOUND);

  wire_case = 25;
  struct hostent *dns_he = LIBC_NAMESPACE::gethostbyname("Dns-Host.Example.");
  ASSERT_NE(dns_he, nullptr);
  EXPECT_STREQ(dns_he->h_name, "Dns-Host.Example");
  EXPECT_EQ(static_cast<uint8_t>(dns_he->h_addr[0]), uint8_t(198));
  EXPECT_EQ(static_cast<uint8_t>(dns_he->h_addr[3]), uint8_t(9));
  EXPECT_EQ(dns_he->h_addr_list[16], nullptr);
}

TEST_F(LlvmLibcGethostbynameTest, LoopbackUdpSocketTransportAndTimeouts) {
  using namespace LIBC_NAMESPACE::netdb;
  namespace nw = LIBC_NAMESPACE::network;
  using LIBC_NAMESPACE::cpp::array;
  using LIBC_NAMESPACE::cpp::scope_exit;
  using LIBC_NAMESPACE::cpp::span;
  using LIBC_NAMESPACE::net::hton;
  using LIBC_NAMESPACE::net::ntoh;

  ScopedTestDbFile hdb(libc_make_test_file_path("host_udp.test"), "",
                       &set_hosts_path);

  int sfd = LIBC_NAMESPACE::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
  int cfd = -1;
  int cfd6 = -1;
  ASSERT_GE(sfd, 0);
  scope_exit guard([&] {
    if (sfd >= 0)
      LIBC_NAMESPACE::close(sfd);
    if (cfd >= 0)
      nw::close_fd(cfd);
    if (cfd6 >= 0)
      nw::close_fd(cfd6);
  });

  struct sockaddr_in sa{};
  struct sockaddr_in peer{};
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = hton<uint32_t>(0x7f000001U);
  auto *sap = reinterpret_cast<struct sockaddr *>(&sa);
  auto *psp = reinterpret_cast<struct sockaddr *>(&peer);
  socklen_t slen = sizeof(sa);
  socklen_t plen = sizeof(peer);
  ASSERT_EQ(LIBC_NAMESPACE::bind(sfd, sap, slen), 0);
  ASSERT_EQ(LIBC_NAMESPACE::getsockname(sfd, sap, &slen), 0);
  const uint16_t PORT = ntoh(sa.sin_port);

  static constexpr uint8_t LO4[4] = {127, 0, 0, 1};
  static constexpr uint8_t LO6[16] = {0, 0, 0, 0, 0, 0, 0, 0,
                                      0, 0, 0, 0, 0, 0, 0, 1};
  static constexpr uint8_t UNROUTABLE6[16] = {0xfe, 0x80};
  static constexpr uint8_t QN[3] = {1, 'a', 0};

  EXPECT_EQ(get_dns_port(), DNS_DEFAULT_PORT);
  EXPECT_FALSE(nw::poll_fd_ready(-2, POLLIN, 0));
  EXPECT_EQ(nw::open_udp(false, LO4, 0, PORT, -1), -1);
  EXPECT_EQ(nw::open_udp(true, UNROUTABLE6, 0, 53), -1);
  cfd6 = nw::open_udp(true, LO6, 0, PORT);
  EXPECT_GE(cfd6, 0);
  EXPECT_EQ(nw::udp_send(-1, {}), ssize_t(-1));

  cfd = nw::open_udp(false, LO4, 0, PORT);
  ASSERT_GE(cfd, 0);
  array<uint8_t, 24> qpkt{};
  const size_t QL = write_query_packet(0x0022, QN, DNS_TYPE_A, qpkt);
  ASSERT_EQ(nw::udp_send(cfd, {qpkt.data(), QL}), static_cast<ssize_t>(QL));

  uint8_t srx[32]{};
  ASSERT_EQ(LIBC_NAMESPACE::recvfrom(sfd, srx, sizeof(srx), 0, psp, &plen),
            static_cast<ssize_t>(QL));

  // Oversized (>1232B) UDP datagram is detected via MSG_TRUNC.
  array<uint8_t, 1300> big{};
  span<uint8_t> urx = span<uint8_t>(big).subspan(0, DNS_UDP_RX_SIZE);
  ASSERT_EQ(LIBC_NAMESPACE::sendto(sfd, big.data(), big.size(), 0, psp, plen),
            static_cast<ssize_t>(big.size()));
  EXPECT_EQ(nw::udp_poll_recv(cfd, urx, 200).kind, nw::UdpRecvKind::HARD_ERROR);

  ASSERT_EQ(LIBC_NAMESPACE::sendto(sfd, qpkt.data(), QL, 0, psp, plen),
            static_cast<ssize_t>(QL));
  EXPECT_EQ(nw::udp_poll_recv(cfd, urx, 200).kind, nw::UdpRecvKind::PACKET);
  EXPECT_EQ(nw::udp_poll_recv(cfd, urx, 1).kind, nw::UdpRecvKind::TIMEOUT);

  // Closed ephemeral UDP port triggers ECONNREFUSED -> TRY_AGAIN cleanly.
  LIBC_NAMESPACE::close(sfd);
  sfd = -1;
  set_dns_port_override(PORT);
  EXPECT_EQ(LIBC_NAMESPACE::gethostbyname("live.example."), nullptr);
  EXPECT_EQ(*h_errno_addr(), TRY_AGAIN);

  set_dns_wire_hook(
      [](const ScopedIpAddr &, bool, span<const uint8_t> q,
         span<uint8_t>) -> int { return q[13] == '3' ? -3 : -4; });
  DnsIo io{};
  array<struct in_addr, DNS_MAX_ADDRS> dummy_addrs{};
  size_t dummy_count = 0;
  EXPECT_EQ(run_udp_query(io, {}, QN, dummy_addrs, dummy_count),
            DnsStatus::SERVER_FAILURE);
  ScopedIpAddr v6ns{};
  v6ns.is_ipv6 = true;
  EXPECT_EQ(run_udp_query(io, v6ns, QN, dummy_addrs, dummy_count),
            DnsStatus::SERVER_FAILURE);
  EXPECT_EQ(LIBC_NAMESPACE::gethostbyname("3.x"), nullptr);
  EXPECT_EQ(LIBC_NAMESPACE::gethostbyname("4.x"), nullptr);
}

#if defined(LIBC_ADD_NULL_CHECKS)
TEST_F(LlvmLibcGethostbynameTest, NullptrCrash) {
  struct hostent he{};
  struct hostent *res = nullptr;
  char buf[16];

  ASSERT_DEATH(
      [&] {
        LIBC_NAMESPACE::gethostbyname_r(nullptr, &he, buf, sizeof(buf), &res,
                                        nullptr);
      },
      WITH_SIGNAL(-1));
  ASSERT_DEATH(
      [&] {
        LIBC_NAMESPACE::gethostbyname_r("a", nullptr, buf, sizeof(buf), &res,
                                        nullptr);
      },
      WITH_SIGNAL(-1));
  ASSERT_DEATH(
      [&] {
        LIBC_NAMESPACE::gethostbyname_r("a", &he, nullptr, sizeof(buf), &res,
                                        nullptr);
      },
      WITH_SIGNAL(-1));
  ASSERT_DEATH(
      [&] {
        LIBC_NAMESPACE::gethostbyname_r("a", &he, buf, sizeof(buf), nullptr,
                                        nullptr);
      },
      WITH_SIGNAL(-1));
  ASSERT_DEATH([] { LIBC_NAMESPACE::gethostbyname(nullptr); }, WITH_SIGNAL(-1));
}
#endif // LIBC_ADD_NULL_CHECKS
