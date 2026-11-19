//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Unit tests for getaddrinfo.
///
//===----------------------------------------------------------------------===//

#include "hdr/errno_macros.h"
#include "hdr/netdb_macros.h"
#include "hdr/netinet_in_macros.h"
#include "hdr/signal_macros.h"
#include "hdr/stdint_proxy.h"
#include "hdr/sys_socket_macros.h"
#include "hdr/types/in_port_t.h"
#include "hdr/types/size_t.h"
#include "hdr/types/socklen_t.h"
#include "hdr/types/struct_addrinfo.h"
#include "hdr/types/struct_sockaddr.h"
#include "hdr/types/struct_sockaddr_in.h"
#include "hdr/types/struct_sockaddr_in6.h"
#include "src/__support/CPP/scope.h"
#include "src/__support/CPP/span.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/OSUtil/network.h"
#include "src/__support/net/byte_order.h"
#include "src/__support/netdb/addrinfo.h"
#include "src/__support/netdb/dns_client.h"
#include "src/__support/netdb/dns_response.h"
#include "src/__support/netdb/dns_wire.h"
#include "src/__support/netdb/hosts.h"
#include "src/__support/netdb/resolv_conf.h"
#include "src/__support/netdb/services.h"
#include "src/netdb/getaddrinfo.h"
#include "src/string/memory_utils/inline_memcpy.h"
#include "src/string/string_utils.h"
#include "src/sys/socket/bind.h"
#include "src/sys/socket/getsockname.h"
#include "src/sys/socket/listen.h"
#include "src/sys/socket/socket.h"
#include "src/unistd/close.h"
#include "test/UnitTest/ErrnoCheckingTest.h"
#include "test/UnitTest/ErrnoSetterMatcher.h"
#include "test/UnitTest/Test.h"
#include "test/src/netdb/test_db_file.h"

using namespace LIBC_NAMESPACE::testing::ErrnoSetterMatcher;

class LlvmLibcGetaddrinfoTest
    : public LIBC_NAMESPACE::testing::ErrnoCheckingTest {
protected:
  void TearDown() override {
    LIBC_NAMESPACE::netdb::clear_addrinfo_alloc_budget();
    LIBC_NAMESPACE::network::clear_addrconfig_override();
    LIBC_NAMESPACE::netdb::set_dns_wire_hook(nullptr);
    LIBC_NAMESPACE::netdb::set_dns_port_override(0);
    LIBC_NAMESPACE::netdb::set_hosts_path(nullptr);
    LIBC_NAMESPACE::netdb::set_resolv_conf_path(nullptr);
    LIBC_NAMESPACE::netdb::set_services_path(nullptr);
    ErrnoCheckingTest::TearDown();
  }
};

TEST_F(LlvmLibcGetaddrinfoTest, ValidationAndErrorCodes) {
  ScopedTestDbFile sdb(libc_make_test_file_path("getaddrinfo_serv.test"),
                       "http 80/tcp www\ntftp 69/udp\n",
                       &LIBC_NAMESPACE::netdb::set_services_path);

  struct addrinfo *raw = nullptr;

  EXPECT_EQ(LIBC_NAMESPACE::getaddrinfo(nullptr, nullptr, nullptr, &raw),
            EAI_NONAME);
  EXPECT_EQ(raw, nullptr);

  struct addrinfo hints{};
  hints.ai_flags = AI_CANONNAME;
  EXPECT_EQ(LIBC_NAMESPACE::getaddrinfo(nullptr, "80", &hints, &raw),
            EAI_BADFLAGS);

  struct HintCase {
    int flags;
    int family;
    int socktype;
    int protocol;
    int expected;
  };
  static constexpr HintCase ERROR_CASES[] = {
      {-1, AF_UNSPEC, 0, 0, EAI_BADFLAGS},
      {0, AF_UNIX, 0, 0, EAI_FAMILY},
      {0, AF_INET6, 0, 0, EAI_NONAME},
      {0, AF_UNSPEC, SOCK_STREAM, IPPROTO_UDP, EAI_SOCKTYPE},
      {0, AF_UNSPEC, SOCK_DGRAM, IPPROTO_TCP, EAI_SOCKTYPE},
      {0, AF_UNSPEC, SOCK_RAW, -1, EAI_SOCKTYPE},
      {0, AF_UNSPEC, SOCK_RAW, IPPROTO_ICMP, EAI_SERVICE},
  };
  for (const HintCase &C : ERROR_CASES) {
    hints = {};
    hints.ai_flags = C.flags;
    hints.ai_family = C.family;
    hints.ai_socktype = C.socktype;
    hints.ai_protocol = C.protocol;
    EXPECT_EQ(LIBC_NAMESPACE::getaddrinfo("127.0.0.1", "80", &hints, &raw),
              C.expected);
    EXPECT_EQ(raw, nullptr);
  }

  static constexpr const char *BAD_SERVICES[] = {
      "", "/", "70000", "80x", "unknown",
  };
  for (const char *serv_str : BAD_SERVICES) {
    EXPECT_EQ(LIBC_NAMESPACE::getaddrinfo("127.0.0.1", serv_str, nullptr, &raw),
              EAI_SERVICE);
  }

  hints = {};
  hints.ai_flags = AI_NUMERICSERV;
  EXPECT_EQ(LIBC_NAMESPACE::getaddrinfo("127.0.0.1", "http", &hints, &raw),
            EAI_NONAME);

  EXPECT_EQ(LIBC_NAMESPACE::getaddrinfo("host.invalid", "80", nullptr, &raw),
            EAI_NONAME);
}

TEST_F(LlvmLibcGetaddrinfoTest, Ipv4LiteralsServicesPassiveCanonAndOom) {
  using LIBC_NAMESPACE::net::hton;
  using LIBC_NAMESPACE::netdb::AddrinfoPtr;

  ScopedTestDbFile sdb(libc_make_test_file_path("getaddrinfo_serv_v4.test"),
                       "http 80/tcp www\ntftp 69/udp\n",
                       &LIBC_NAMESPACE::netdb::set_services_path);

  struct addrinfo *raw = nullptr;
  ASSERT_EQ(LIBC_NAMESPACE::getaddrinfo("192.0.2.9", "www", nullptr, &raw), 0);
  AddrinfoPtr res(raw);
  ASSERT_NE(res.get(), nullptr);
  EXPECT_EQ(res->ai_family, AF_INET);
  EXPECT_EQ(res->ai_socktype, SOCK_STREAM);
  EXPECT_EQ(res->ai_protocol, IPPROTO_TCP);
  EXPECT_EQ(res->ai_next, nullptr);

  const auto *sa = reinterpret_cast<const struct sockaddr_in *>(res->ai_addr);
  ASSERT_NE(sa, nullptr);
  EXPECT_EQ(static_cast<int>(sa->sin_family), AF_INET);
  EXPECT_EQ(sa->sin_port, hton<in_port_t>(80));
  EXPECT_EQ(sa->sin_addr.s_addr, hton<uint32_t>(0xc0000209U));

  struct addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  ASSERT_EQ(LIBC_NAMESPACE::getaddrinfo("127.0.0.1", "tftp", &hints, &raw), 0);
  res.reset(raw);
  EXPECT_EQ(res->ai_socktype, SOCK_DGRAM);
  EXPECT_EQ(
      reinterpret_cast<const struct sockaddr_in *>(res->ai_addr)->sin_port,
      hton<in_port_t>(69));

  hints = {};
  hints.ai_flags = AI_CANONNAME | AI_NUMERICHOST;
  ASSERT_EQ(LIBC_NAMESPACE::getaddrinfo("192.0.2.1", "443", &hints, &raw), 0);
  res.reset(raw);
  EXPECT_STREQ(res->ai_canonname, "192.0.2.1");
  ASSERT_NE(res->ai_next, nullptr);
  EXPECT_EQ(res->ai_next->ai_canonname, nullptr);

  hints = {};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  ASSERT_EQ(LIBC_NAMESPACE::getaddrinfo("127.0.0.1", nullptr, &hints, &raw), 0);
  res.reset(raw);
  EXPECT_EQ(res->ai_socktype, SOCK_STREAM);
  EXPECT_EQ(res->ai_next, nullptr);

  hints = {};
  hints.ai_family = AF_INET;
  hints.ai_protocol = IPPROTO_TCP;
  ASSERT_EQ(LIBC_NAMESPACE::getaddrinfo("127.0.0.1", nullptr, &hints, &raw), 0);
  res.reset(raw);
  EXPECT_EQ(res->ai_socktype, SOCK_STREAM);
  EXPECT_EQ(res->ai_next, nullptr);

  hints = {};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_RAW;
  hints.ai_protocol = IPPROTO_ICMP;
  ASSERT_EQ(LIBC_NAMESPACE::getaddrinfo("127.0.0.1", nullptr, &hints, &raw), 0);
  res.reset(raw);
  EXPECT_EQ(res->ai_socktype, SOCK_RAW);
  EXPECT_EQ(res->ai_protocol, IPPROTO_ICMP);

  ASSERT_EQ(LIBC_NAMESPACE::getaddrinfo("127.0.0.1", nullptr, nullptr, &raw),
            0);
  res.reset(raw);
  ASSERT_NE(res->ai_next, nullptr);
  ASSERT_NE(res->ai_next->ai_next, nullptr);
  EXPECT_EQ(res->ai_next->ai_next->ai_socktype, SOCK_RAW);

  ASSERT_EQ(LIBC_NAMESPACE::getaddrinfo(nullptr, "80", nullptr, &raw), 0);
  res.reset(raw);
  EXPECT_EQ(res->ai_family, AF_INET);
  EXPECT_EQ(reinterpret_cast<const struct sockaddr_in *>(res->ai_addr)
                ->sin_addr.s_addr,
            hton<uint32_t>(0x7f000001U));

  hints = {};
  hints.ai_flags = AI_PASSIVE;
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  ASSERT_EQ(LIBC_NAMESPACE::getaddrinfo(nullptr, "80", &hints, &raw), 0);
  res.reset(raw);
  EXPECT_EQ(reinterpret_cast<const struct sockaddr_in *>(res->ai_addr)
                ->sin_addr.s_addr,
            uint32_t(0));

  for (size_t budget = 0; budget < 2; ++budget) {
    LIBC_NAMESPACE::netdb::set_addrinfo_alloc_budget(budget);
    EXPECT_EQ(LIBC_NAMESPACE::getaddrinfo("127.0.0.1", "80", nullptr, &raw),
              EAI_MEMORY);
    EXPECT_EQ(raw, nullptr);
  }
}

TEST_F(LlvmLibcGetaddrinfoTest, Ipv6LiteralsScopeV4MappedAndServiceErrors) {
  using LIBC_NAMESPACE::net::hton;
  using LIBC_NAMESPACE::netdb::AddrinfoPtr;

  ScopedTestDbFile sdb(libc_make_test_file_path("getaddrinfo_serv_v6.test"),
                       "http 80/tcp\n",
                       &LIBC_NAMESPACE::netdb::set_services_path);

  struct addrinfo *raw = nullptr;
  struct addrinfo hints{};
  hints.ai_flags = AI_PASSIVE;
  hints.ai_family = AF_INET6;
  hints.ai_socktype = SOCK_STREAM;
  ASSERT_EQ(LIBC_NAMESPACE::getaddrinfo(nullptr, "http", &hints, &raw), 0);
  AddrinfoPtr res(raw);
  ASSERT_NE(res.get(), nullptr);
  const auto *s6 = reinterpret_cast<const struct sockaddr_in6 *>(res->ai_addr);
  EXPECT_EQ(static_cast<int>(s6->sin6_family), AF_INET6);
  EXPECT_EQ(s6->sin6_port, hton<in_port_t>(80));
  EXPECT_EQ(s6->sin6_addr.s6_addr[15], uint8_t(0));

  hints = {};
  hints.ai_family = AF_INET;
  EXPECT_EQ(LIBC_NAMESPACE::getaddrinfo("::1", "80", &hints, &raw), EAI_NONAME);
  EXPECT_EQ(
      LIBC_NAMESPACE::getaddrinfo("fe80::1%no_such_if0", "80", nullptr, &raw),
      EAI_NONAME);
  EXPECT_EQ(LIBC_NAMESPACE::getaddrinfo("fe80::1%0123456789abcdef", "80",
                                        nullptr, &raw),
            EAI_NONAME);
  EXPECT_EQ(LIBC_NAMESPACE::getaddrinfo("fe80::1%", "80", nullptr, &raw),
            EAI_NONAME);
  EXPECT_EQ(LIBC_NAMESPACE::network::if_nametoindex("lo", -1), uint32_t(0));

  ASSERT_EQ(LIBC_NAMESPACE::getaddrinfo("fe80::1%lo", "80", nullptr, &raw), 0);
  res.reset(raw);
  s6 = reinterpret_cast<const struct sockaddr_in6 *>(res->ai_addr);
  EXPECT_GT(s6->sin6_scope_id, uint32_t(0));
  EXPECT_EQ(
      LIBC_NAMESPACE::getaddrinfo("fe80::1%4294967296", "80", nullptr, &raw),
      EAI_NONAME);

  ASSERT_EQ(LIBC_NAMESPACE::getaddrinfo("fe80::1%2", "80", nullptr, &raw), 0);
  res.reset(raw);
  s6 = reinterpret_cast<const struct sockaddr_in6 *>(res->ai_addr);
  EXPECT_EQ(s6->sin6_scope_id, uint32_t(2));
  EXPECT_EQ(s6->sin6_addr.s6_addr[0], uint8_t(0xfe));

  ASSERT_EQ(
      LIBC_NAMESPACE::getaddrinfo("::ffff:192.0.2.1", "80", nullptr, &raw), 0);
  res.reset(raw);
  s6 = reinterpret_cast<const struct sockaddr_in6 *>(res->ai_addr);
  EXPECT_EQ(s6->sin6_addr.s6_addr[10], uint8_t(0xff));
  EXPECT_EQ(s6->sin6_addr.s6_addr[15], uint8_t(1));

  hints = {};
  hints.ai_flags = AI_V4MAPPED;
  hints.ai_family = AF_INET6;
  hints.ai_socktype = SOCK_STREAM;
  ASSERT_EQ(LIBC_NAMESPACE::getaddrinfo("192.0.2.1", "80", &hints, &raw), 0);
  res.reset(raw);
  s6 = reinterpret_cast<const struct sockaddr_in6 *>(res->ai_addr);
  EXPECT_EQ(s6->sin6_addr.s6_addr[10], uint8_t(0xff));
  EXPECT_EQ(s6->sin6_addr.s6_addr[11], uint8_t(0xff));
  EXPECT_EQ(s6->sin6_addr.s6_addr[15], uint8_t(1));

  hints = {};
  hints.ai_family = AF_INET6;
  hints.ai_socktype = SOCK_DGRAM;
  ASSERT_EQ(LIBC_NAMESPACE::getaddrinfo("::1", "80", &hints, &raw), 0);
  res.reset(raw);
  EXPECT_EQ(res->ai_socktype, SOCK_DGRAM);

  LIBC_NAMESPACE::netdb::set_services_path("/dev/null/not_a_dir");
  hints = {};
  hints.ai_family = AF_INET6;
  hints.ai_socktype = SOCK_STREAM;
  EXPECT_THAT(LIBC_NAMESPACE::getaddrinfo("::1", "http", &hints, &raw),
              Fails<int>(ENOTDIR, EAI_SYSTEM));
}

TEST_F(LlvmLibcGetaddrinfoTest, HostsDatabaseRfc6761AndOverlongLines) {
  using LIBC_NAMESPACE::cpp::span;
  using LIBC_NAMESPACE::cpp::string_view;
  using LIBC_NAMESPACE::net::hton;
  using LIBC_NAMESPACE::netdb::AddrinfoPtr;

  LIBC_NAMESPACE::netdb::set_resolv_conf_path(
      libc_make_test_file_path("gai_resolv_missing.test"));
  LIBC_NAMESPACE::netdb::set_dns_wire_hook(
      [](const LIBC_NAMESPACE::netdb::ScopedIpAddr &, bool,
         span<const uint8_t> q, span<uint8_t> r) -> int {
        using namespace LIBC_NAMESPACE::netdb;
        LIBC_NAMESPACE::inline_memcpy(r.data(), q.data(), q.size());
        write_u16_be(r, 2, DNS_FLAG_QR | 3);
        return static_cast<int>(q.size());
      });

  struct addrinfo *raw = nullptr;

  // Exercise default nullptr hosts_path with RFC 6761 localhost synthesis.
  ASSERT_EQ(LIBC_NAMESPACE::getaddrinfo("hermetic-probe.localhost", "80",
                                        nullptr, &raw),
            0);
  AddrinfoPtr res(raw);
  ASSERT_NE(res.get(), nullptr);

  // Missing /etc/hosts file falls back cleanly (ENOENT is not EAI_SYSTEM).
  LIBC_NAMESPACE::netdb::set_hosts_path(
      libc_make_test_file_path("gai_hosts_missing.test"));
  EXPECT_EQ(LIBC_NAMESPACE::getaddrinfo("missing.local", "80", nullptr, &raw),
            EAI_NONAME);

  char db[2200];
  size_t pos = 0;
  auto append = [&](string_view s) {
    for (char ch : s)
      db[pos++] = ch;
  };

  // Blank, comment, single-token, bad-IP, and embedded-NUL records.
  static constexpr char BAD_NUL[] = "10.0.0.99 canon.local bad\0nul\n";
  static constexpr char OK_COMMENT_NUL[] =
      "10.0.0.2 canon.local ca other alias1 # ok\0comment\n";
  append("\n# comment line\n10.0.0.1\nbad_ip canon.local\n");
  append(string_view(BAD_NUL, sizeof(BAD_NUL) - 1));
  append(string_view(OK_COMMENT_NUL, sizeof(OK_COMMENT_NUL) - 1));
  append("fe80::1%lo canon.local alias1\n");
  append("10.0.0.9 v4.local\n");

  // Overlong (>64B) IP token on a matching hostname line.
  for (size_t i = 0; i < 66; ++i)
    db[pos++] = '0';
  append(" canon.local\n");

  // Overlong (>253B) canonical hostname on a matching alias line.
  append("10.0.0.8 ");
  for (size_t i = 0; i < 254; ++i)
    db[pos++] = 'q';
  append(" canon.local\n");

  // Overlong (>600B) /etc/hosts line with target alias past byte 600.
  append("10.42.0.1 wide.local");
  for (size_t i = 0; i < 30; ++i)
    append(" filler_alias_padding");
  append(" tail-600.local\n");

  // Multi-homed (>16 addresses) entry to exercise HOSTS_MAX_ADDRS cap.
  for (size_t i = 0; i < 18; ++i)
    append("10.1.0.1 multihomed.local\n");

  ScopedTestDbFile hdb(libc_make_test_file_path("gai_hosts.test"),
                       string_view(db, pos),
                       &LIBC_NAMESPACE::netdb::set_hosts_path);

  struct addrinfo hints{};
  hints.ai_flags = AI_NUMERICHOST;
  EXPECT_EQ(LIBC_NAMESPACE::getaddrinfo("canon.local", "80", &hints, &raw),
            EAI_NONAME);

  char lbl64[65]{};
  char dom254[255]{};
  for (size_t i = 0; i < 64; ++i)
    lbl64[i] = 'a';
  for (size_t i = 0; i < 254; ++i)
    dom254[i] = (i % 50 == 49) ? '.' : 'b';

  static constexpr const char *STATIC_BAD_NAMES[] = {
      "",          "..a",   "a b",  "a\x7f",      "invalid",     "invalid.",
      "x.invalid", "h.123", "h.1a", "xlocalhost", "a.localhosx", "no",
  };
  for (const char *bad : STATIC_BAD_NAMES) {
    EXPECT_EQ(LIBC_NAMESPACE::getaddrinfo(bad, "80", nullptr, &raw),
              EAI_NONAME);
  }
  EXPECT_EQ(LIBC_NAMESPACE::getaddrinfo(lbl64, "80", nullptr, &raw),
            EAI_NONAME);
  EXPECT_EQ(LIBC_NAMESPACE::getaddrinfo(dom254, "80", nullptr, &raw),
            EAI_NONAME);

  // Case-insensitive alias + trailing dot match returns canonical name.
  hints = {};
  hints.ai_flags = AI_CANONNAME;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  ASSERT_EQ(LIBC_NAMESPACE::getaddrinfo("ALIAS1.", "80", &hints, &raw), 0);
  res.reset(raw);
  EXPECT_STREQ(res->ai_canonname, "canon.local");
  EXPECT_EQ(res->ai_family, AF_INET);
  ASSERT_NE(res->ai_next, nullptr);
  EXPECT_EQ(res->ai_next->ai_family, AF_INET6);

  // Alias beyond byte 600 on a >600-byte /etc/hosts line resolves cleanly.
  ASSERT_EQ(LIBC_NAMESPACE::getaddrinfo("TAIL-600.LOCAL", "80", &hints, &raw),
            0);
  res.reset(raw);
  EXPECT_STREQ(res->ai_canonname, "wide.local");
  EXPECT_EQ(reinterpret_cast<const struct sockaddr_in *>(res->ai_addr)
                ->sin_addr.s_addr,
            hton<uint32_t>(0x0a2a0001U));

  static constexpr int FAMILIES[3] = {AF_UNSPEC, AF_INET, AF_INET6};
  for (int fam : FAMILIES) {
    hints = {};
    hints.ai_flags = AI_CANONNAME;
    hints.ai_family = fam;
    hints.ai_socktype = SOCK_STREAM;
    ASSERT_EQ(LIBC_NAMESPACE::getaddrinfo("canon.local", "80", &hints, &raw),
              0);
    res.reset(raw);

    ASSERT_EQ(LIBC_NAMESPACE::getaddrinfo("Sub.LOCALHOST.", "80", &hints, &raw),
              0);
    res.reset(raw);
    EXPECT_STREQ(res->ai_canonname, "Sub.LOCALHOST");
  }

  ASSERT_EQ(
      LIBC_NAMESPACE::getaddrinfo("multihomed.local", "80", nullptr, &raw), 0);
  res.reset(raw);

  // AI_V4MAPPED without AI_ALL drops IPv4 when native IPv6 exists; AI_ALL keeps
  // both.
  hints = {};
  hints.ai_flags = AI_V4MAPPED;
  hints.ai_family = AF_INET6;
  hints.ai_socktype = SOCK_STREAM;
  ASSERT_EQ(LIBC_NAMESPACE::getaddrinfo("canon.local", "80", &hints, &raw), 0);
  res.reset(raw);
  EXPECT_EQ(res->ai_next, nullptr);

  // When only IPv4 exists, AI_V4MAPPED maps it to ::ffff:a.b.c.d.
  ASSERT_EQ(LIBC_NAMESPACE::getaddrinfo("v4.local", "80", &hints, &raw), 0);
  res.reset(raw);
  EXPECT_EQ(res->ai_family, AF_INET6);

  hints.ai_flags = AI_V4MAPPED | AI_ALL;
  ASSERT_EQ(LIBC_NAMESPACE::getaddrinfo("canon.local", "80", &hints, &raw), 0);
  res.reset(raw);
  ASSERT_NE(res->ai_next, nullptr);

  // Live Netlink RTM_GETADDR probe and zero-init override masks.
  hints = {};
  hints.ai_flags = AI_ADDRCONFIG;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  const int LIVE_RC =
      LIBC_NAMESPACE::getaddrinfo("canon.local", "80", &hints, &raw);
  if (LIVE_RC == 0)
    res.reset(raw);

  static constexpr uint8_t MASKS[4] = {0, 1, 2, 3};
  for (uint8_t m : MASKS) {
    LIBC_NAMESPACE::network::set_addrconfig_override(m);
    const int RC =
        LIBC_NAMESPACE::getaddrinfo("canon.local", "80", &hints, &raw);
    if (RC == 0)
      res.reset(raw);
    EXPECT_EQ(RC, m == 0 ? EAI_NONAME : 0);
  }
  LIBC_NAMESPACE::network::clear_addrconfig_override();

  // Exercise Netlink socket/sendto/recvfrom fallbacks and malformed frames.
  bool allow4 = false;
  bool allow6 = false;
  LIBC_NAMESPACE::network::probe_netlink_addrconfig(allow4, allow6, -1);
  EXPECT_TRUE(allow4 && allow6);
  LIBC_NAMESPACE::network::probe_netlink_addrconfig(allow4, allow6, AF_UNIX);
  EXPECT_FALSE(allow4 || allow6);
  LIBC_NAMESPACE::network::probe_netlink_addrconfig(allow4, allow6, AF_NETLINK,
                                                    1);
  EXPECT_FALSE(allow4 || allow6);

  EXPECT_FALSE(LIBC_NAMESPACE::network::is_non_loopback_v4({}));
  static constexpr uint8_t ZERO4[4] = {0, 0, 0, 0};
  EXPECT_FALSE(LIBC_NAMESPACE::network::is_non_loopback_v4(ZERO4));
  EXPECT_FALSE(LIBC_NAMESPACE::network::is_non_loopback_v6({}));
  static constexpr uint8_t TWO6[16] = {0, 0, 0, 0, 0, 0, 0, 0,
                                       0, 0, 0, 0, 0, 0, 0, 2};
  EXPECT_TRUE(LIBC_NAMESPACE::network::is_non_loopback_v6(TWO6));

  // Synthetic Netlink frames: shortlen, overlong, NLMSG_ERROR, short
  // RTM_NEWADDR, bad rta_len, unknown family, and multi-message step.
  static constexpr uint8_t SHORT_NL[16] = {8};
  static constexpr uint8_t OVER_NL[16] = {32};
  static constexpr uint8_t ERR_NL[16] = {16, 0, 0, 0, 2, 0};
  EXPECT_FALSE(LIBC_NAMESPACE::network::parse_addrconfig_batch(SHORT_NL, allow4,
                                                               allow6));
  EXPECT_FALSE(
      LIBC_NAMESPACE::network::parse_addrconfig_batch(OVER_NL, allow4, allow6));
  EXPECT_TRUE(
      LIBC_NAMESPACE::network::parse_addrconfig_batch(ERR_NL, allow4, allow6));

  static constexpr uint8_t EDGE_BATCH[] = {
      // Msg 1 (len 20, padded 20): RTM_NEWADDR (20) shorter than ifaddrmsg (24)
      20,
      0,
      0,
      0,
      20,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      // Msg 2 (len 36): RTM_NEWADDR (20), family AF_UNIX (1), 2 RTAs
      36,
      0,
      0,
      0,
      20,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      1,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      8,
      0,
      1,
      0,
      0,
      0,
      0,
      0,
      4,
      0,
      2,
      0,
      // Msg 3 (len 28): RTM_NEWADDR (20), bad rta_len = 2
      28,
      0,
      0,
      0,
      20,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      2,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      2,
      0,
      1,
      0,
      // Msg 4 (len 28): RTM_NEWADDR (20), overlong rta_len = 12
      28,
      0,
      0,
      0,
      20,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      2,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      12,
      0,
      1,
      0,
      // Msg 5 (len 16): unknown type 99
      16,
      0,
      0,
      0,
      99,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
  };
  EXPECT_FALSE(LIBC_NAMESPACE::network::parse_addrconfig_batch(EDGE_BATCH,
                                                               allow4, allow6));

  // Open failure (ENOTDIR) and read failure (EISDIR) report EAI_SYSTEM + errno.
  char notdir_path[280];
  const size_t BASE_LEN = LIBC_NAMESPACE::internal::strlcpy(
      notdir_path, hdb.get_path(), sizeof(notdir_path));
  LIBC_NAMESPACE::internal::strlcpy(notdir_path + BASE_LEN, "/hosts",
                                    sizeof(notdir_path) - BASE_LEN);
  LIBC_NAMESPACE::netdb::set_hosts_path(notdir_path);
  EXPECT_THAT(LIBC_NAMESPACE::getaddrinfo("canon.local", "80", nullptr, &raw),
              Fails<int>(ENOTDIR, EAI_SYSTEM));

  LIBC_NAMESPACE::netdb::set_hosts_path(".");
  EXPECT_THAT(LIBC_NAMESPACE::getaddrinfo("canon.local", "80", nullptr, &raw),
              Fails<int>(EISDIR, EAI_SYSTEM));
}

TEST_F(LlvmLibcGetaddrinfoTest, LiveDualStackDnsAndTcpFallback) {
  using namespace LIBC_NAMESPACE::netdb;
  using LIBC_NAMESPACE::cpp::scope_exit;
  using LIBC_NAMESPACE::cpp::span;
  using LIBC_NAMESPACE::net::hton;
  using LIBC_NAMESPACE::net::ntoh;

  ScopedTestDbFile hdb(libc_make_test_file_path("gai_dns_hosts.test"), "",
                       &set_hosts_path);
  ScopedTestDbFile rdb(libc_make_test_file_path("gai_dns_resolv.test"),
                       "nameserver 127.0.0.1\noptions attempts:1 timeout:1\n",
                       &set_resolv_conf_path);

  int tfd = LIBC_NAMESPACE::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
  ASSERT_GE(tfd, 0);
  scope_exit close_tfd([&] {
    if (tfd >= 0)
      LIBC_NAMESPACE::close(tfd);
  });

  struct sockaddr_in sa{};
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = hton<uint32_t>(0x7f000001U);
  auto *sap = reinterpret_cast<struct sockaddr *>(&sa);
  socklen_t slen = sizeof(sa);
  ASSERT_EQ(LIBC_NAMESPACE::bind(tfd, sap, slen), 0);
  ASSERT_EQ(LIBC_NAMESPACE::getsockname(tfd, sap, &slen), 0);
  ASSERT_EQ(LIBC_NAMESPACE::listen(tfd, 1), 0);
  LIBC_NAMESPACE::close(tfd);
  tfd = -1;
  set_dns_port_override(ntoh(sa.sin_port));

  ScopedTestDbFile rvc(libc_make_test_file_path("gai_dns_vc.test"),
                       "nameserver 127.0.0.1\noptions use-vc attempts:1\n",
                       &set_resolv_conf_path);
  struct addrinfo hints{};
  hints.ai_flags = AI_CANONNAME;
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo *raw = nullptr;
  EXPECT_EQ(LIBC_NAMESPACE::getaddrinfo("tcp.example.", "80", &hints, &raw),
            EAI_AGAIN);
  set_resolv_conf_path(rdb.get_path());

  set_dns_wire_hook([](const ScopedIpAddr &, bool is_tcp, span<const uint8_t> q,
                       span<uint8_t> r) -> int {
    const char C = static_cast<char>(q[13]);
    const int Q = static_cast<int>(q.size());
    const uint16_t QT = read_u16_be(q, static_cast<size_t>(Q - 4));
    LIBC_NAMESPACE::inline_memcpy(r.data(), q.data(), q.size());

    if (!is_tcp && C == 't') {
      write_u16_be(r, 2, DNS_FLAG_QR | DNS_FLAG_TC);
      return Q;
    }
    if (C == '2' || C == '5' || C == '0' || (C == '4' && QT == DNS_TYPE_AAAA)) {
      const uint16_t RC = C == '4' ? 0 : static_cast<uint16_t>(C - '0');
      write_u16_be(r, 2, static_cast<uint16_t>(DNS_FLAG_QR | RC));
      return Q;
    }
    if (C == 'x') {
      // Malformed AAAA (RDLEN = 3 != 16) -> IGNORE -> RETRY -> EAI_AGAIN.
      write_u16_be(r, 2, DNS_FLAG_QR);
      write_u16_be(r, 6, 1);
      const uint8_t BAD_AAAA[15] = {
          0xc0, 12, 0, static_cast<uint8_t>(QT), 0, 1, 0, 0, 0, 5, 0, 3,
          1,    2,  3,
      };
      LIBC_NAMESPACE::inline_memcpy(r.data() + Q, BAD_AAAA, sizeof(BAD_AAAA));
      return Q + 15;
    }

    write_u16_be(r, 2, DNS_FLAG_QR);
    write_u16_be(r, 6, 2);
    const uint16_t RL = QT == DNS_TYPE_AAAA ? 16 : 4;
    uint8_t rr[43] = {
        1, 'r', 0, 0, static_cast<uint8_t>(QT), 0, 1, 0,
        0, 0,   5, 0, static_cast<uint8_t>(RL),
    };
    rr[13] = QT == DNS_TYPE_AAAA ? 0x20 : 203;
    rr[13 + RL - 1] = 7;
    const uint8_t CN[14] = {
        0xc0, 12, 0, 5, 0, 1, 0, 0, 0, 5, 0, 2, 0xc0, static_cast<uint8_t>(Q),
    };
    LIBC_NAMESPACE::inline_memcpy(r.data() + Q, rr, 13 + RL);
    LIBC_NAMESPACE::inline_memcpy(r.data() + Q + 13 + RL, CN, sizeof(CN));
    return Q + 27 + RL;
  });

  // Dual-stack AF_UNSPEC with TC=1 TCP fallback + CNAME ai_canonname.
  ASSERT_EQ(LIBC_NAMESPACE::getaddrinfo("t.example", "80", &hints, &raw), 0);
  AddrinfoPtr res(raw);
  ASSERT_NE(res.get(), nullptr);
  EXPECT_STREQ(res->ai_canonname, "r");
  EXPECT_EQ(res->ai_family, AF_INET6);
  ASSERT_NE(res->ai_next, nullptr);
  EXPECT_EQ(res->ai_next->ai_family, AF_INET);

  hints = {};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  ASSERT_EQ(LIBC_NAMESPACE::getaddrinfo("d.example", "80", &hints, &raw), 0);
  res.reset(raw);
  EXPECT_EQ(res->ai_family, AF_INET);

  hints = {};
  hints.ai_family = AF_INET6;
  hints.ai_socktype = SOCK_STREAM;
  ASSERT_EQ(LIBC_NAMESPACE::getaddrinfo("d.example", "80", &hints, &raw), 0);
  res.reset(raw);
  EXPECT_EQ(res->ai_family, AF_INET6);

  hints = {};
  hints.ai_flags = AI_V4MAPPED;
  hints.ai_family = AF_INET6;
  hints.ai_socktype = SOCK_STREAM;
  ASSERT_EQ(LIBC_NAMESPACE::getaddrinfo("d.example", "80", &hints, &raw), 0);
  res.reset(raw);
  EXPECT_EQ(res->ai_next, nullptr);

  ASSERT_EQ(LIBC_NAMESPACE::getaddrinfo("4.example", "80", &hints, &raw), 0);
  res.reset(raw);
  EXPECT_EQ(reinterpret_cast<const struct sockaddr_in6 *>(res->ai_addr)
                ->sin6_addr.s6_addr[10],
            uint8_t(0xff));

  hints.ai_flags = AI_V4MAPPED | AI_ALL;
  ASSERT_EQ(LIBC_NAMESPACE::getaddrinfo("d.example", "80", &hints, &raw), 0);
  res.reset(raw);
  ASSERT_NE(res->ai_next, nullptr);

  EXPECT_EQ(LIBC_NAMESPACE::getaddrinfo("2.example", "80", nullptr, &raw),
            EAI_AGAIN);
  EXPECT_EQ(LIBC_NAMESPACE::getaddrinfo("x.example", "80", nullptr, &raw),
            EAI_AGAIN);
  EXPECT_EQ(LIBC_NAMESPACE::getaddrinfo("5.example", "80", nullptr, &raw),
            EAI_FAIL);
  EXPECT_EQ(LIBC_NAMESPACE::getaddrinfo("0.example", "80", nullptr, &raw),
            EAI_NONAME);

  char notdir_path[280];
  const size_t BASE_LEN = LIBC_NAMESPACE::internal::strlcpy(
      notdir_path, rdb.get_path(), sizeof(notdir_path));
  LIBC_NAMESPACE::internal::strlcpy(notdir_path + BASE_LEN, "/resolv.conf",
                                    sizeof(notdir_path) - BASE_LEN);
  set_resolv_conf_path(notdir_path);
  EXPECT_THAT(LIBC_NAMESPACE::getaddrinfo("d.example", "80", nullptr, &raw),
              Fails<int>(ENOTDIR, EAI_SYSTEM));
}

#if defined(LIBC_ADD_NULL_CHECKS)
TEST_F(LlvmLibcGetaddrinfoTest, NullResCrash) {
  ASSERT_DEATH(
      [] { LIBC_NAMESPACE::getaddrinfo("127.0.0.1", "80", nullptr, nullptr); },
      WITH_SIGNAL(-1));
}
#endif // LIBC_ADD_NULL_CHECKS
