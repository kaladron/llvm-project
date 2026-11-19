//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Unit tests for gethostbyname2_r and gethostbyname2.
///
//===----------------------------------------------------------------------===//

#include "hdr/errno_macros.h"
#include "hdr/netdb_macros.h"
#include "hdr/signal_macros.h"
#include "hdr/stdint_proxy.h"
#include "hdr/sys_socket_macros.h"
#include "hdr/types/size_t.h"
#include "hdr/types/struct_hostent.h"
#include "src/__support/CPP/span.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/netdb/dns_client.h"
#include "src/__support/netdb/dns_response.h"
#include "src/__support/netdb/dns_wire.h"
#include "src/__support/netdb/h_errno.h"
#include "src/__support/netdb/hosts.h"
#include "src/__support/netdb/resolv_conf.h"
#include "src/netdb/gethostbyname2.h"
#include "src/netdb/gethostbyname2_r.h"
#include "src/string/memory_utils/inline_memcpy.h"
#include "test/UnitTest/ErrnoCheckingTest.h"
#include "test/UnitTest/Test.h"
#include "test/src/netdb/test_db_file.h"

namespace {

constexpr void make_v6_alias(char (&out)[16], size_t index) {
  constexpr LIBC_NAMESPACE::cpp::string_view PREFIX = "v6_alias_";
  for (size_t i = 0; i < PREFIX.size(); ++i)
    out[i] = PREFIX[i];
  out[PREFIX.size()] = static_cast<char>('0' + (index / 10));
  out[PREFIX.size() + 1] = static_cast<char>('0' + (index % 10));
  out[PREFIX.size() + 2] = '\0';
}

} // namespace

class LlvmLibcGethostbyname2Test
    : public LIBC_NAMESPACE::testing::ErrnoCheckingTest {
protected:
  void TearDown() override {
    LIBC_NAMESPACE::netdb::set_dns_wire_hook(nullptr);
    LIBC_NAMESPACE::netdb::set_hosts_path(nullptr);
    LIBC_NAMESPACE::netdb::set_resolv_conf_path(nullptr);
    ErrnoCheckingTest::TearDown();
  }
};

TEST_F(LlvmLibcGethostbyname2Test, LiteralsFamiliesAndHostsUncappedAliases) {
  using LIBC_NAMESPACE::cpp::span;
  using LIBC_NAMESPACE::cpp::string_view;

  LIBC_NAMESPACE::netdb::set_dns_wire_hook(
      [](const LIBC_NAMESPACE::netdb::ScopedIpAddr &, bool,
         span<const uint8_t> q, span<uint8_t> r) -> int {
        using namespace LIBC_NAMESPACE::netdb;
        LIBC_NAMESPACE::inline_memcpy(r.data(), q.data(), q.size());
        write_u16_be(r, 2, DNS_FLAG_QR | DNS_RCODE_NXDOMAIN);
        return static_cast<int>(q.size());
      });

  struct hostent he{};
  struct hostent *res = &he;
  alignas(char *) char buf[1024];
  int herr = 0;

  // Unsupported address families return EAFNOSUPPORT and NO_RECOVERY.
  EXPECT_EQ(LIBC_NAMESPACE::gethostbyname2_r("app", AF_UNIX, &he, buf,
                                             sizeof(buf), &res, &herr),
            EAFNOSUPPORT);
  EXPECT_EQ(res, nullptr);
  EXPECT_EQ(herr, NO_RECOVERY);
  EXPECT_EQ(LIBC_NAMESPACE::gethostbyname2_r("app", -1, &he, buf, sizeof(buf),
                                             &res, nullptr),
            EAFNOSUPPORT);
  EXPECT_EQ(LIBC_NAMESPACE::gethostbyname2("app", AF_UNIX), nullptr);
  EXPECT_EQ(*LIBC_NAMESPACE::netdb::h_errno_addr(), NO_RECOVERY);

  // Numeric IPv6 and IPv4 literals bypass /etc/hosts and DNS.
  ASSERT_EQ(LIBC_NAMESPACE::gethostbyname2_r("2001:db8::9", AF_INET6, &he, buf,
                                             sizeof(buf), &res, &herr),
            0);
  ASSERT_EQ(res, &he);
  EXPECT_STREQ(he.h_name, "2001:db8::9");
  EXPECT_EQ(he.h_addrtype, AF_INET6);
  EXPECT_EQ(he.h_length, 16);
  EXPECT_EQ(he.h_aliases[0], nullptr);
  EXPECT_EQ(static_cast<uint8_t>(he.h_addr[0]), uint8_t(0x20));
  EXPECT_EQ(static_cast<uint8_t>(he.h_addr[15]), uint8_t(9));
  EXPECT_EQ(he.h_addr_list[1], nullptr);

  ASSERT_EQ(LIBC_NAMESPACE::gethostbyname2_r("198.51.100.4", AF_INET, &he, buf,
                                             sizeof(buf), &res, &herr),
            0);
  ASSERT_EQ(res, &he);
  EXPECT_EQ(he.h_addrtype, AF_INET);
  EXPECT_EQ(he.h_length, 4);
  EXPECT_EQ(static_cast<uint8_t>(he.h_addr[3]), uint8_t(4));

  // Cross-family literals fail cleanly with HOST_NOT_FOUND.
  EXPECT_EQ(LIBC_NAMESPACE::gethostbyname2("198.51.100.4", AF_INET6), nullptr);
  EXPECT_EQ(*LIBC_NAMESPACE::netdb::h_errno_addr(), HOST_NOT_FOUND);
  EXPECT_EQ(LIBC_NAMESPACE::gethostbyname2("2001:db8::9", AF_INET), nullptr);
  EXPECT_EQ(*LIBC_NAMESPACE::netdb::h_errno_addr(), HOST_NOT_FOUND);
  EXPECT_EQ(LIBC_NAMESPACE::gethostbyname2("fe80::zz", AF_INET6), nullptr);
  EXPECT_EQ(*LIBC_NAMESPACE::netdb::h_errno_addr(), HOST_NOT_FOUND);

  constexpr size_t TWENTY_ALIASES = 20;
  char db[2048];
  size_t pos = 0;
  auto append = [&](string_view s) {
    for (char ch : s)
      db[pos++] = ch;
  };

  append("999.999.999.999 badip.local\n");
  append("10.1.2.3 dual.local dual4\n");
  append("2001:db8::1 dual.local dual6 a6\n");
  append("fe80::2%1 dual.local\n");
  append("2001:db8::20 wide6.local");
  for (size_t i = 0; i < TWENTY_ALIASES; ++i) {
    char alias[16];
    make_v6_alias(alias, i);
    db[pos++] = ' ';
    append(string_view(alias, 11));
  }
  for (size_t i = 0; i < 40; ++i)
    append("          ");
  db[pos++] = '\n';

  ScopedTestDbFile hdb(libc_make_test_file_path("host2_r.test"),
                       string_view(db, pos),
                       &LIBC_NAMESPACE::netdb::set_hosts_path);

  // Malformed IP line in /etc/hosts is skipped cleanly.
  EXPECT_EQ(LIBC_NAMESPACE::gethostbyname2_r("badip.local", AF_INET6, &he, buf,
                                             sizeof(buf), &res, &herr),
            0);
  EXPECT_EQ(res, nullptr);
  EXPECT_EQ(herr, HOST_NOT_FOUND);

  // Dual-stack /etc/hosts returns only the requested address family.
  ASSERT_EQ(LIBC_NAMESPACE::gethostbyname2_r("DUAL.LOCAL", AF_INET6, &he, buf,
                                             sizeof(buf), &res, &herr),
            0);
  ASSERT_EQ(res, &he);
  EXPECT_STREQ(he.h_name, "dual.local");
  EXPECT_STREQ(he.h_aliases[0], "dual6");
  EXPECT_STREQ(he.h_aliases[1], "a6");
  EXPECT_EQ(he.h_aliases[2], nullptr);
  EXPECT_EQ(static_cast<uint8_t>(he.h_addr_list[0][15]), uint8_t(1));
  ASSERT_NE(he.h_addr_list[1], nullptr);
  EXPECT_EQ(static_cast<uint8_t>(he.h_addr_list[1][0]), uint8_t(0xfe));
  EXPECT_EQ(static_cast<uint8_t>(he.h_addr_list[1][15]), uint8_t(2));
  EXPECT_EQ(he.h_addr_list[2], nullptr);

  // ERANGE with zero-length nullptr buffer and tight capacities.
  herr = 0;
  EXPECT_EQ(LIBC_NAMESPACE::gethostbyname2_r("dual.local", AF_INET6, &he,
                                             nullptr, 0, &res, &herr),
            ERANGE);
  EXPECT_EQ(res, nullptr);
  EXPECT_EQ(herr, NETDB_INTERNAL);

  for (size_t cap = 1; cap < 96; ++cap) {
    herr = 0;
    const int RC = LIBC_NAMESPACE::gethostbyname2_r("dual.local", AF_INET6, &he,
                                                    buf, cap, &res, &herr);
    if (RC != 0) {
      EXPECT_EQ(RC, ERANGE);
      EXPECT_EQ(res, nullptr);
      EXPECT_EQ(herr, NETDB_INTERNAL);
    }
  }

  // >512B 20-alias IPv6 /etc/hosts line resolves all 20 aliases.
  struct hostent *tls = LIBC_NAMESPACE::gethostbyname2("V6_ALIAS_19", AF_INET6);
  ASSERT_NE(tls, nullptr);
  EXPECT_STREQ(tls->h_name, "wide6.local");
  for (size_t i = 0; i < TWENTY_ALIASES; ++i) {
    char expected[16];
    make_v6_alias(expected, i);
    ASSERT_NE(tls->h_aliases[i], nullptr);
    EXPECT_STREQ(tls->h_aliases[i], expected);
  }
  EXPECT_EQ(tls->h_aliases[TWENTY_ALIASES], nullptr);

  // RFC 6761 localhost synthesis for AF_INET6 and AF_INET.
  tls = LIBC_NAMESPACE::gethostbyname2("sub.localhost.", AF_INET6);
  ASSERT_NE(tls, nullptr);
  EXPECT_STREQ(tls->h_name, "sub.localhost");
  EXPECT_EQ(tls->h_addrtype, AF_INET6);
  EXPECT_EQ(static_cast<uint8_t>(tls->h_addr[15]), uint8_t(1));

  tls = LIBC_NAMESPACE::gethostbyname2("sub.localhost.", AF_INET);
  ASSERT_NE(tls, nullptr);
  EXPECT_EQ(tls->h_addrtype, AF_INET);
  EXPECT_EQ(static_cast<uint8_t>(tls->h_addr[0]), uint8_t(127));
  EXPECT_EQ(static_cast<uint8_t>(tls->h_addr[3]), uint8_t(1));
}

TEST_F(LlvmLibcGethostbyname2Test, DnsQuadACnameAndNoData) {
  using namespace LIBC_NAMESPACE::netdb;
  using LIBC_NAMESPACE::cpp::span;

  ScopedTestDbFile hdb(libc_make_test_file_path("host2_dns.test"), "",
                       &set_hosts_path);
  ScopedTestDbFile rdb(libc_make_test_file_path("host2_resolv.test"),
                       "nameserver 127.0.0.1\noptions attempts:1 timeout:1\n",
                       &set_resolv_conf_path);

  set_dns_wire_hook([](const ScopedIpAddr &, bool, span<const uint8_t> q,
                       span<uint8_t> r) -> int {
    const size_t Q = q.size();
    LIBC_NAMESPACE::inline_memcpy(r.data(), q.data(), Q);
    if (q[13] == 'n') {
      // Valid response with IN A record for an AAAA query -> NODATA / NO_DATA.
      write_u16_be(r, 2, DNS_FLAG_QR);
      write_u16_be(r, 6, 1);
      const uint8_t ONLY_A[16] = {
          0xc0, 12, 0, 1, 0, 1, 0, 0, 0, 5, 0, 4, 192, 0, 2, 1,
      };
      LIBC_NAMESPACE::inline_memcpy(r.data() + Q, ONLY_A, sizeof(ONLY_A));
      return static_cast<int>(Q + sizeof(ONLY_A));
    }

    // CNAME -> AAAA chain.
    write_u16_be(r, 2, DNS_FLAG_QR);
    write_u16_be(r, 6, 2);
    const uint8_t V6_CHAIN[44] = {
        0xc0, 12,   0, 5, 0,  1, 0, 0, 0, 5, 0, 3, 1,  'z',  0,
        1,    'z',  0, 0, 28, 0, 1, 0, 0, 0, 5, 0, 16, 0x20, 1,
        0x0d, 0xb8, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,  7,
    };
    LIBC_NAMESPACE::inline_memcpy(r.data() + Q, V6_CHAIN, sizeof(V6_CHAIN));
    return static_cast<int>(Q + sizeof(V6_CHAIN));
  });

  struct hostent *t6 =
      LIBC_NAMESPACE::gethostbyname2("dns6.example.", AF_INET6);
  ASSERT_NE(t6, nullptr);
  EXPECT_STREQ(t6->h_name, "z");
  EXPECT_EQ(t6->h_addrtype, AF_INET6);
  EXPECT_EQ(t6->h_length, 16);
  EXPECT_EQ(static_cast<uint8_t>(t6->h_addr[0]), uint8_t(0x20));
  EXPECT_EQ(static_cast<uint8_t>(t6->h_addr[15]), uint8_t(7));

  EXPECT_EQ(LIBC_NAMESPACE::gethostbyname2("n.example.", AF_INET6), nullptr);
  EXPECT_EQ(*h_errno_addr(), NO_DATA);
}

#ifdef LIBC_ADD_NULL_CHECKS
TEST_F(LlvmLibcGethostbyname2Test, NullptrCrash) {
  struct hostent he{};
  struct hostent *r = nullptr;
  char b[8]{};
  EXPECT_DEATH([] { LIBC_NAMESPACE::gethostbyname2(nullptr, AF_INET6); },
               WITH_SIGNAL(-1));
  EXPECT_DEATH(
      [&] {
        LIBC_NAMESPACE::gethostbyname2_r(nullptr, AF_INET6, &he, b, sizeof(b),
                                         &r, nullptr);
      },
      WITH_SIGNAL(-1));
  EXPECT_DEATH(
      [&] {
        LIBC_NAMESPACE::gethostbyname2_r("a", AF_INET6, nullptr, b, sizeof(b),
                                         &r, nullptr);
      },
      WITH_SIGNAL(-1));
  EXPECT_DEATH(
      [&] {
        LIBC_NAMESPACE::gethostbyname2_r("a", AF_INET6, &he, nullptr, sizeof(b),
                                         &r, nullptr);
      },
      WITH_SIGNAL(-1));
  EXPECT_DEATH(
      [&] {
        LIBC_NAMESPACE::gethostbyname2_r("a", AF_INET6, &he, b, sizeof(b),
                                         nullptr, nullptr);
      },
      WITH_SIGNAL(-1));
}
#endif // LIBC_ADD_NULL_CHECKS
