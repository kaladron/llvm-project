//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Unit tests for getnameinfo.
///
//===----------------------------------------------------------------------===//

#include "hdr/errno_macros.h"
#include "hdr/netdb_macros.h"
#include "hdr/signal_macros.h"
#include "hdr/stdint_proxy.h"
#include "hdr/sys_socket_macros.h"
#include "hdr/types/size_t.h"
#include "hdr/types/socklen_t.h"
#include "hdr/types/struct_sockaddr.h"
#include "hdr/types/struct_sockaddr_in.h"
#include "hdr/types/struct_sockaddr_in6.h"
#include "src/__support/CPP/span.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/OSUtil/network.h"
#include "src/__support/net/byte_order.h"
#include "src/__support/netdb/dns_client.h"
#include "src/__support/netdb/dns_response.h"
#include "src/__support/netdb/dns_wire.h"
#include "src/__support/netdb/hosts.h"
#include "src/__support/netdb/resolv_conf.h"
#include "src/__support/netdb/services.h"
#include "src/netdb/getnameinfo.h"
#include "src/string/memory_utils/inline_memcpy.h"
#include "src/string/string_utils.h"
#include "test/UnitTest/ErrnoCheckingTest.h"
#include "test/UnitTest/ErrnoSetterMatcher.h"
#include "test/src/netdb/test_db_file.h"

namespace {

int nxdomain_dns_hook(const LIBC_NAMESPACE::netdb::ScopedIpAddr &, bool,
                      LIBC_NAMESPACE::cpp::span<const uint8_t> q,
                      LIBC_NAMESPACE::cpp::span<uint8_t> r) {
  using namespace LIBC_NAMESPACE::netdb;
  LIBC_NAMESPACE::inline_memcpy(r.data(), q.data(), q.size());
  write_u16_be(r, 2, DNS_FLAG_QR | DNS_RCODE_NXDOMAIN);
  return static_cast<int>(q.size());
}

int mock_ptr_dns_hook(const LIBC_NAMESPACE::netdb::ScopedIpAddr &, bool,
                      LIBC_NAMESPACE::cpp::span<const uint8_t> q,
                      LIBC_NAMESPACE::cpp::span<uint8_t> r) {
  using namespace LIBC_NAMESPACE::netdb;
  using LIBC_NAMESPACE::cpp::span;

  size_t pos = q.size();
  LIBC_NAMESPACE::inline_memcpy(r.data(), q.data(), pos);
  const char C = static_cast<char>(q[13]);

  auto add_ptr = [&](uint16_t owner_off, span<const uint8_t> target) {
    write_u16_be(r, pos, static_cast<uint16_t>(0xc000u | owner_off));
    write_u16_be(r, pos + 2, DNS_TYPE_PTR);
    write_u16_be(r, pos + 4, DNS_CLASS_IN);
    write_u16_be(r, pos + 6, 0);
    write_u16_be(r, pos + 8, 5);
    write_u16_be(r, pos + 10, static_cast<uint16_t>(target.size()));
    LIBC_NAMESPACE::inline_memcpy(r.data() + pos + 12, target.data(),
                                  target.size());
    pos += 12 + target.size();
  };

  if (C == '4' || C == '6' || C == '8') {
    uint16_t rc = 0;
    if (C == '8')
      rc = DNS_RCODE_SERVFAIL;
    else if (C == '6')
      rc = 5;
    write_u16_be(r, 2, static_cast<uint16_t>(DNS_FLAG_QR | rc));
    return static_cast<int>(pos);
  }

  static constexpr uint8_t V6_NAME[] = {2, 'v', '6', 4, 'c', 'o', 'r', 'p', 0};
  static constexpr uint8_t ROOT_NAME[] = {0};
  static constexpr uint8_t BAD_NAME[] = {
      7, 'b', 'a', 'd', '_', 'p', 't', 'r', 4, 'c', 'o', 'r', 'p', 0,
  };

  if (C == '5') {
    write_u16_be(r, 2, DNS_FLAG_QR);
    write_u16_be(r, 6, 1);
    add_ptr(12, ROOT_NAME);
    return static_cast<int>(pos);
  }

  if (C == '3') {
    write_u16_be(r, 2, DNS_FLAG_QR);
    write_u16_be(r, 6, 1);
    add_ptr(12, BAD_NAME);
    return static_cast<int>(pos);
  }

  if (C == '2' || C == '9') {
    write_u16_be(r, 2, DNS_FLAG_QR);
    write_u16_be(r, 6, 2);
    add_ptr(12, V6_NAME);
    add_ptr(12, V6_NAME);
    return static_cast<int>(pos);
  }

  if (C == '7') {
    const uint8_t RRS[] = {
        1,
        '7',
        0,
        0,
        12,
        0,
        1,
        0,
        0,
        0,
        5,
        0,
        10,
        3,
        'w',
        'e',
        'b',
        4,
        'c',
        'o',
        'r',
        'p',
        0,
        0xc0,
        12,
        0,
        5,
        0,
        1,
        0,
        0,
        0,
        5,
        0,
        2,
        0xc0,
        static_cast<uint8_t>(pos),
    };
    write_u16_be(r, 2, DNS_FLAG_QR);
    write_u16_be(r, 6, 2);
    LIBC_NAMESPACE::inline_memcpy(r.data() + pos, RRS, sizeof(RRS));
    return static_cast<int>(pos + sizeof(RRS));
  }

  write_u16_be(r, 2, DNS_FLAG_QR | DNS_RCODE_NXDOMAIN);
  return static_cast<int>(pos);
}

} // namespace

class LlvmLibcGetnameinfoTest
    : public LIBC_NAMESPACE::testing::ErrnoCheckingTest {
protected:
  void TearDown() override {
    LIBC_NAMESPACE::netdb::set_dns_wire_hook(nullptr);
    LIBC_NAMESPACE::netdb::set_hosts_path(nullptr);
    LIBC_NAMESPACE::netdb::set_resolv_conf_path(nullptr);
    LIBC_NAMESPACE::netdb::set_services_path(nullptr);
    ErrnoCheckingTest::TearDown();
  }
};

TEST_F(LlvmLibcGetnameinfoTest, NumericIpv4ServicesAndValidation) {
  using LIBC_NAMESPACE::cpp::string_view;
  using LIBC_NAMESPACE::net::hton;

  ScopedTestDbFile rdb(libc_make_test_file_path("gni_nx_resolv.test"),
                       "nameserver 127.0.0.1\noptions attempts:1 timeout:1\n",
                       &LIBC_NAMESPACE::netdb::set_resolv_conf_path);
  LIBC_NAMESPACE::netdb::set_dns_wire_hook(&nxdomain_dns_hook);

  char host[NI_MAXHOST];
  char serv[NI_MAXSERV];
  char ifbuf[16];
  EXPECT_FALSE(LIBC_NAMESPACE::network::if_indextoname(1, ifbuf, -1));

  struct sockaddr_in s4{};
  s4.sin_family = AF_INET;
  s4.sin_port = hton<uint16_t>(443);
  s4.sin_addr.s_addr = hton<uint32_t>(0xc00002feU);
  const auto *sa4 = reinterpret_cast<const struct sockaddr *>(&s4);

  // Default nullptr hosts_path exercises /etc/hosts fallback path cleanly.
  ASSERT_EQ(LIBC_NAMESPACE::getnameinfo(sa4, sizeof(s4), host, sizeof(host),
                                        nullptr, 0, 0),
            0);

  // Missing /etc/hosts file falls back cleanly to numeric formatting.
  LIBC_NAMESPACE::netdb::set_hosts_path(
      libc_make_test_file_path("gni_missing_hosts.test"));
  s4.sin_addr.s_addr = hton<uint32_t>(0x0a000005U);
  ASSERT_EQ(LIBC_NAMESPACE::getnameinfo(sa4, sizeof(s4), host, sizeof(host),
                                        nullptr, 0, 0),
            0);
  EXPECT_STREQ(host, "10.0.0.5");

  char hbuf[1600];
  size_t hp = 0;
  auto append = [&](string_view s) {
    for (char ch : s)
      hbuf[hp++] = ch;
  };

  append("# comment\n\n");
  append("10.0.0.1\n");
  for (size_t i = 0; i < 66; ++i)
    hbuf[hp++] = 'a';
  append(" over_ip.local\n");
  append("bad_ip x.local\n");
  append("10.0.0.99 other.local\n");
  append(":: unspec.local\n");
  append("10.0.0.5 box.local\n");
  append("fe80::2 bare6.local\n");
  append("fe80::3%lo scoped6.local\n");

  // Embedded NUL before '#' or '\n' must be rejected.
  append("10.0.0.12");
  hbuf[hp++] = '\0';
  append(" nul.local\n");
  append("10.0.0.13 nul2.local bad");
  hbuf[hp++] = '\0';
  append("alias\n");

  // Overlong >253-character canonical hostname must be rejected.
  append("10.0.0.6 ");
  for (size_t i = 0; i < 254; ++i)
    hbuf[hp++] = 'a';
  hbuf[hp++] = '\n';

  // >600-byte /etc/hosts reverse line must still resolve its canonical name.
  append("10.0.0.7 wide-rev.local");
  for (size_t i = 0; i < 65; ++i)
    append(" alias_pad_");
  hbuf[hp++] = '\n';

  ScopedTestDbFile hdb(libc_make_test_file_path("gni_hosts.test"),
                       string_view(hbuf, hp),
                       &LIBC_NAMESPACE::netdb::set_hosts_path);
  ScopedTestDbFile sdb(libc_make_test_file_path("gni_serv.test"),
                       "https 443/tcp\ndomain 53/udp\n",
                       &LIBC_NAMESPACE::netdb::set_services_path);

  EXPECT_EQ(LIBC_NAMESPACE::getnameinfo(sa4, sizeof(s4), host, sizeof(host),
                                        serv, sizeof(serv), 0x4000),
            EAI_BADFLAGS);
  EXPECT_EQ(LIBC_NAMESPACE::getnameinfo(sa4, sizeof(s4), host, sizeof(host),
                                        serv, sizeof(serv),
                                        NI_NUMERICHOST | NI_NAMEREQD),
            EAI_BADFLAGS);
  EXPECT_EQ(
      LIBC_NAMESPACE::getnameinfo(sa4, sizeof(s4), nullptr, 0, nullptr, 0, 0),
      EAI_NONAME);
  EXPECT_EQ(LIBC_NAMESPACE::getnameinfo(sa4, sizeof(s4), host, 0, serv, 0, 0),
            EAI_NONAME);
  EXPECT_EQ(
      LIBC_NAMESPACE::getnameinfo(sa4, 4, host, sizeof(host), nullptr, 0, 0),
      EAI_FAMILY);

  s4.sin_family = AF_UNIX;
  EXPECT_EQ(LIBC_NAMESPACE::getnameinfo(sa4, sizeof(s4), host, sizeof(host),
                                        nullptr, 0, 0),
            EAI_FAMILY);
  s4.sin_family = AF_INET;

  EXPECT_EQ(LIBC_NAMESPACE::getnameinfo(sa4, sizeof(s4), host, 2, serv,
                                        sizeof(serv), 0),
            EAI_OVERFLOW);
  EXPECT_EQ(
      LIBC_NAMESPACE::getnameinfo(sa4, sizeof(s4), nullptr, 8, serv, 2, 0),
      EAI_OVERFLOW);

  ASSERT_EQ(LIBC_NAMESPACE::getnameinfo(sa4, sizeof(s4), host, sizeof(host),
                                        serv, sizeof(serv), 0),
            0);
  EXPECT_STREQ(host, "box.local");
  EXPECT_STREQ(serv, "https");

  ASSERT_EQ(LIBC_NAMESPACE::getnameinfo(sa4, sizeof(s4), host, sizeof(host),
                                        nullptr, 8, NI_NUMERICHOST),
            0);
  EXPECT_STREQ(host, "10.0.0.5");
  EXPECT_EQ(LIBC_NAMESPACE::getnameinfo(sa4, sizeof(s4), host, 2, nullptr, 0,
                                        NI_NUMERICHOST),
            EAI_OVERFLOW);

  // Verify >600B /etc/hosts reverse line resolves and embedded NULs are
  // rejected.
  s4.sin_addr.s_addr = hton<uint32_t>(0x0a000007U);
  ASSERT_EQ(LIBC_NAMESPACE::getnameinfo(sa4, sizeof(s4), host, sizeof(host),
                                        nullptr, 0, NI_NAMEREQD),
            0);
  EXPECT_STREQ(host, "wide-rev.local");

  s4.sin_addr.s_addr = hton<uint32_t>(0x0a00000cU);
  EXPECT_EQ(LIBC_NAMESPACE::getnameinfo(sa4, sizeof(s4), host, sizeof(host),
                                        nullptr, 0, NI_NAMEREQD),
            EAI_NONAME);
  s4.sin_addr.s_addr = hton<uint32_t>(0x0a00000dU);
  EXPECT_EQ(LIBC_NAMESPACE::getnameinfo(sa4, sizeof(s4), host, sizeof(host),
                                        nullptr, 0, NI_NAMEREQD),
            EAI_NONAME);
  s4.sin_addr.s_addr = hton<uint32_t>(0x0a000006U);
  EXPECT_EQ(LIBC_NAMESPACE::getnameinfo(sa4, sizeof(s4), host, sizeof(host),
                                        nullptr, 0, NI_NAMEREQD),
            EAI_NONAME);

  s4.sin_addr.s_addr = hton<uint32_t>(0x0a000005U);
  ASSERT_EQ(LIBC_NAMESPACE::getnameinfo(sa4, sizeof(s4), nullptr, 8, serv,
                                        sizeof(serv), NI_NUMERICSERV),
            0);
  EXPECT_STREQ(serv, "443");

  s4.sin_port = hton<uint16_t>(53);
  ASSERT_EQ(LIBC_NAMESPACE::getnameinfo(sa4, sizeof(s4), nullptr, 0, serv,
                                        sizeof(serv), NI_DGRAM),
            0);
  EXPECT_STREQ(serv, "domain");

  ASSERT_EQ(LIBC_NAMESPACE::getnameinfo(sa4, sizeof(s4), nullptr, 0, serv,
                                        sizeof(serv), 0),
            0);
  EXPECT_STREQ(serv, "53");
}

TEST_F(LlvmLibcGetnameinfoTest, Ipv6ScopesEmbeddedV4AndSystemErrors) {
  using LIBC_NAMESPACE::net::hton;
  using LIBC_NAMESPACE::testing::ErrnoSetterMatcher::Fails;

  ScopedTestDbFile rdb(libc_make_test_file_path("gni_v6_resolv.test"),
                       "nameserver 127.0.0.1\noptions attempts:1 timeout:1\n",
                       &LIBC_NAMESPACE::netdb::set_resolv_conf_path);
  LIBC_NAMESPACE::netdb::set_dns_wire_hook(&nxdomain_dns_hook);

  ScopedTestDbFile hdb(libc_make_test_file_path("gni_v6_hosts.test"),
                       ":: unspec.local\n10.0.0.5 box.local\n"
                       "fe80::2 bare6.local\nfe80::3%lo scoped6.local\n",
                       &LIBC_NAMESPACE::netdb::set_hosts_path);
  ScopedTestDbFile sdb(libc_make_test_file_path("gni_v6_serv.test"),
                       "https 443/tcp\n",
                       &LIBC_NAMESPACE::netdb::set_services_path);

  char host[NI_MAXHOST];
  char serv[NI_MAXSERV];
  struct sockaddr_in6 s6{};
  s6.sin6_family = AF_INET6;
  const auto *sa6 = reinterpret_cast<const struct sockaddr *>(&s6);
  const uint32_t LO_IDX = LIBC_NAMESPACE::network::if_nametoindex("lo");

  EXPECT_EQ(
      LIBC_NAMESPACE::getnameinfo(sa6, 20, host, sizeof(host), nullptr, 0, 0),
      EAI_FAMILY);
  EXPECT_EQ(LIBC_NAMESPACE::getnameinfo(sa6, sizeof(s6), host, sizeof(host),
                                        nullptr, 0, NI_NAMEREQD),
            EAI_NONAME);
  EXPECT_EQ(
      LIBC_NAMESPACE::getnameinfo(sa6, sizeof(s6), host, 2, nullptr, 0, 0),
      EAI_OVERFLOW);
  ASSERT_EQ(LIBC_NAMESPACE::getnameinfo(sa6, sizeof(s6), host, sizeof(host),
                                        nullptr, 0, 0),
            0);
  EXPECT_STREQ(host, "::");

  struct EmbeddedCase {
    uint8_t b10;
    uint8_t b11;
    uint8_t b12;
    uint8_t b15;
    const char *expected;
  };
  static constexpr EmbeddedCase CASES[7] = {
      {0, 0, 0, 1, "::1"},        {0, 0, 0, 3, "0.0.0.3"},
      {0, 0, 10, 5, "box.local"}, {0xff, 0xff, 10, 5, "box.local"},
      {0xff, 0, 0, 0, nullptr},   {1, 0, 0, 0, nullptr},
      {0, 1, 0, 0, nullptr},
  };
  for (const EmbeddedCase &C : CASES) {
    s6.sin6_addr = {};
    s6.sin6_addr.s6_addr[10] = C.b10;
    s6.sin6_addr.s6_addr[11] = C.b11;
    s6.sin6_addr.s6_addr[12] = C.b12;
    s6.sin6_addr.s6_addr[15] = C.b15;
    ASSERT_EQ(LIBC_NAMESPACE::getnameinfo(sa6, sizeof(s6), host, sizeof(host),
                                          nullptr, 0, 0),
              0);
    if (C.expected != nullptr)
      EXPECT_STREQ(host, C.expected);
  }

  s6.sin6_addr = {};
  s6.sin6_addr.s6_addr[0] = 0xfe;
  s6.sin6_addr.s6_addr[1] = 0x80;
  s6.sin6_addr.s6_addr[15] = 2;
  s6.sin6_scope_id = LO_IDX;
  ASSERT_EQ(LIBC_NAMESPACE::getnameinfo(sa6, sizeof(s6), host, sizeof(host),
                                        nullptr, 0, 0),
            0);
  EXPECT_STREQ(host, "bare6.local");

  s6.sin6_addr.s6_addr[15] = 3;
  s6.sin6_scope_id = 0;
  ASSERT_EQ(LIBC_NAMESPACE::getnameinfo(sa6, sizeof(s6), host, sizeof(host),
                                        nullptr, 0, 0),
            0);
  EXPECT_STREQ(host, "scoped6.local");

  s6.sin6_scope_id = LO_IDX;
  ASSERT_EQ(LIBC_NAMESPACE::getnameinfo(sa6, sizeof(s6), host, sizeof(host),
                                        nullptr, 0, 0),
            0);
  EXPECT_STREQ(host, "scoped6.local");

  ASSERT_EQ(LIBC_NAMESPACE::getnameinfo(sa6, sizeof(s6), host, sizeof(host),
                                        nullptr, 0, NI_NUMERICHOST),
            0);
  EXPECT_STREQ(host, "fe80::3%lo");
  EXPECT_EQ(LIBC_NAMESPACE::getnameinfo(sa6, sizeof(s6), host, 6, nullptr, 0,
                                        NI_NUMERICHOST),
            EAI_OVERFLOW);

  ASSERT_EQ(LIBC_NAMESPACE::getnameinfo(sa6, sizeof(s6), host, sizeof(host),
                                        nullptr, 0,
                                        NI_NUMERICHOST | NI_NUMERICSCOPE),
            0);

  s6.sin6_scope_id = 999999;
  ASSERT_EQ(LIBC_NAMESPACE::getnameinfo(sa6, sizeof(s6), host, sizeof(host),
                                        nullptr, 0, 0),
            0);
  EXPECT_STREQ(host, "fe80::3%999999");

  struct sockaddr_in s4{};
  s4.sin_family = AF_INET;
  s4.sin_port = hton<uint16_t>(443);
  s4.sin_addr.s_addr = hton<uint32_t>(0x0a000005U);
  const auto *sa4 = reinterpret_cast<const struct sockaddr *>(&s4);

  char notdir_path[280];
  const size_t BASE_LEN = LIBC_NAMESPACE::internal::strlcpy(
      notdir_path, hdb.get_path(), sizeof(notdir_path));
  LIBC_NAMESPACE::internal::strlcpy(notdir_path + BASE_LEN, "/hosts",
                                    sizeof(notdir_path) - BASE_LEN);
  LIBC_NAMESPACE::netdb::set_hosts_path(notdir_path);
  EXPECT_THAT(LIBC_NAMESPACE::getnameinfo(sa4, sizeof(s4), host, sizeof(host),
                                          nullptr, 0, 0),
              Fails<int>(ENOTDIR, EAI_SYSTEM));

  LIBC_NAMESPACE::netdb::set_hosts_path(".");
  EXPECT_THAT(LIBC_NAMESPACE::getnameinfo(sa4, sizeof(s4), host, sizeof(host),
                                          nullptr, 0, 0),
              Fails<int>(EISDIR, EAI_SYSTEM));

  LIBC_NAMESPACE::netdb::set_services_path(notdir_path);
  EXPECT_THAT(LIBC_NAMESPACE::getnameinfo(sa4, sizeof(s4), nullptr, 0, serv,
                                          sizeof(serv), 0),
              Fails<int>(ENOTDIR, EAI_SYSTEM));
}

TEST_F(LlvmLibcGetnameinfoTest, DnsPtrRfc2317NofqdnAndHostValidation) {
  using LIBC_NAMESPACE::net::hton;
  using LIBC_NAMESPACE::testing::ErrnoSetterMatcher::Fails;

  ScopedTestDbFile hdb(libc_make_test_file_path("gni_ptr_hosts.test"),
                       "10.0.0.8 host8.corp.\n"
                       "10.0.0.9 single\n"
                       "10.0.0.10 .corp\n"
                       "10.0.0.11 a.other\n",
                       &LIBC_NAMESPACE::netdb::set_hosts_path);
  ScopedTestDbFile rdb_empty(libc_make_test_file_path("gni_ptr_r0.test"),
                             "nameserver 127.0.0.1\noptions attempts:1\n",
                             &LIBC_NAMESPACE::netdb::set_resolv_conf_path);
  LIBC_NAMESPACE::netdb::set_dns_wire_hook(&mock_ptr_dns_hook);

  char host[NI_MAXHOST];
  struct sockaddr_in s4{};
  s4.sin_family = AF_INET;
  const auto *sa4 = reinterpret_cast<const struct sockaddr *>(&s4);

  auto query_v4 = [&](uint32_t ip, int flags) {
    s4.sin_addr.s_addr = hton(ip);
    return LIBC_NAMESPACE::getnameinfo(sa4, sizeof(s4), host, sizeof(host),
                                       nullptr, 0, flags);
  };

  // NI_NOFQDN with empty local search domain strips only trailing dot.
  ASSERT_EQ(query_v4(0x0a000008U, NI_NOFQDN), 0);
  EXPECT_STREQ(host, "host8.corp");

  ScopedTestDbFile rdb_search(libc_make_test_file_path("gni_ptr_r1.test"),
                              "nameserver 10.0.0.1\nnameserver 10.0.0.2\n"
                              "search corp.\noptions rotate attempts:1\n",
                              &LIBC_NAMESPACE::netdb::set_resolv_conf_path);

  struct OkCase {
    uint32_t ip;
    int flags;
    const char *expected;
  };
  static constexpr OkCase OK_CASES[] = {
      {0x0a000008U, NI_NOFQDN, "host8"}, {0x0a000009U, NI_NOFQDN, "single"},
      {0x0a00000aU, NI_NOFQDN, ".corp"}, {0x0a00000bU, NI_NOFQDN, "a.other"},
      {0xc0000207U, 0, "web.corp"},      {0xc0000207U, NI_NOFQDN, "web"},
      {0xc0000202U, NI_NOFQDN, "v6"},    {0xc0000204U, 0, "192.0.2.4"},
      {0xc0000205U, 0, "192.0.2.5"},     {0xc0000203U, 0, "192.0.2.3"},
  };
  for (const OkCase &C : OK_CASES) {
    ASSERT_EQ(query_v4(C.ip, C.flags), 0);
    EXPECT_STREQ(host, C.expected);
  }

  EXPECT_EQ(query_v4(0xc0000208U, 0), EAI_AGAIN);
  EXPECT_EQ(query_v4(0xc0000206U, 0), EAI_FAIL);
  EXPECT_EQ(query_v4(0xc0000204U, NI_NAMEREQD), EAI_NONAME);
  EXPECT_EQ(query_v4(0xc0000205U, NI_NAMEREQD), EAI_NONAME);
  EXPECT_EQ(query_v4(0xc0000203U, NI_NAMEREQD), EAI_NONAME);

  struct sockaddr_in6 s6{};
  s6.sin6_family = AF_INET6;
  s6.sin6_addr.s6_addr[0] = 0x20;
  s6.sin6_addr.s6_addr[15] = 9;
  const auto *sa6 = reinterpret_cast<const struct sockaddr *>(&s6);
  ASSERT_EQ(LIBC_NAMESPACE::getnameinfo(sa6, sizeof(s6), host, sizeof(host),
                                        nullptr, 0, 0),
            0);
  EXPECT_STREQ(host, "v6.corp");

  char notdir_path[280];
  const size_t BASE_LEN = LIBC_NAMESPACE::internal::strlcpy(
      notdir_path, rdb_search.get_path(), sizeof(notdir_path));
  LIBC_NAMESPACE::internal::strlcpy(notdir_path + BASE_LEN, "/resolv.conf",
                                    sizeof(notdir_path) - BASE_LEN);
  LIBC_NAMESPACE::netdb::set_resolv_conf_path(notdir_path);
  EXPECT_THAT(query_v4(0xc0000207U, 0), Fails<int>(ENOTDIR, EAI_SYSTEM));
}

#ifdef LIBC_ADD_NULL_CHECKS
TEST_F(LlvmLibcGetnameinfoTest, NullptrCrash) {
  char host[8]{};
  EXPECT_DEATH(
      [&] {
        LIBC_NAMESPACE::getnameinfo(nullptr, 16, host, sizeof(host), nullptr, 0,
                                    0);
      },
      WITH_SIGNAL(-1));
}
#endif // LIBC_ADD_NULL_CHECKS
