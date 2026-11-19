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
#include "src/__support/CPP/string_view.h"
#include "src/__support/OSUtil/network.h"
#include "src/__support/net/byte_order.h"
#include "src/__support/netdb/hosts.h"
#include "src/__support/netdb/services.h"
#include "src/netdb/getnameinfo.h"
#include "src/string/string_utils.h"
#include "test/UnitTest/ErrnoCheckingTest.h"
#include "test/UnitTest/ErrnoSetterMatcher.h"
#include "test/src/netdb/test_db_file.h"

class LlvmLibcGetnameinfoTest
    : public LIBC_NAMESPACE::testing::ErrnoCheckingTest {
protected:
  void TearDown() override {
    LIBC_NAMESPACE::netdb::set_hosts_path(nullptr);
    LIBC_NAMESPACE::netdb::set_services_path(nullptr);
    ErrnoCheckingTest::TearDown();
  }
};

TEST_F(LlvmLibcGetnameinfoTest, NumericIpv4ServicesAndValidation) {
  using LIBC_NAMESPACE::cpp::string_view;
  using LIBC_NAMESPACE::net::hton;
  using LIBC_NAMESPACE::testing::ErrnoSetterMatcher::Fails;

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
