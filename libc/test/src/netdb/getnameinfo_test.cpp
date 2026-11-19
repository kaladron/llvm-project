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
#include "src/__support/net/byte_order.h"
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
    LIBC_NAMESPACE::netdb::set_services_path(nullptr);
    ErrnoCheckingTest::TearDown();
  }
};

TEST_F(LlvmLibcGetnameinfoTest, NumericIpv4ServicesAndValidation) {
  using LIBC_NAMESPACE::net::hton;
  using LIBC_NAMESPACE::testing::ErrnoSetterMatcher::Fails;

  ScopedTestDbFile sdb(libc_make_test_file_path("gni_serv.test"),
                       "https 443/tcp\ndomain 53/udp\n",
                       &LIBC_NAMESPACE::netdb::set_services_path);

  char host[NI_MAXHOST];
  char serv[NI_MAXSERV];
  struct sockaddr_in s4{};
  s4.sin_family = AF_INET;
  s4.sin_port = hton<uint16_t>(443);
  s4.sin_addr.s_addr = hton<uint32_t>(0x0a000005U);
  const auto *sa4 = reinterpret_cast<const struct sockaddr *>(&s4);

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

  EXPECT_EQ(LIBC_NAMESPACE::getnameinfo(sa4, sizeof(s4), host, sizeof(host),
                                        nullptr, 8, NI_NAMEREQD),
            EAI_NONAME);
  EXPECT_EQ(LIBC_NAMESPACE::getnameinfo(sa4, sizeof(s4), host, 2, serv,
                                        sizeof(serv), 0),
            EAI_OVERFLOW);
  EXPECT_EQ(
      LIBC_NAMESPACE::getnameinfo(sa4, sizeof(s4), nullptr, 8, serv, 2, 0),
      EAI_OVERFLOW);

  ASSERT_EQ(LIBC_NAMESPACE::getnameinfo(sa4, sizeof(s4), host, sizeof(host),
                                        serv, sizeof(serv), 0),
            0);
  EXPECT_STREQ(host, "10.0.0.5");
  EXPECT_STREQ(serv, "https");

  ASSERT_EQ(LIBC_NAMESPACE::getnameinfo(sa4, sizeof(s4), host, sizeof(host),
                                        nullptr, 8, NI_NUMERICHOST),
            0);
  EXPECT_STREQ(host, "10.0.0.5");

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

  char notdir_path[280];
  const size_t BASE_LEN = LIBC_NAMESPACE::internal::strlcpy(
      notdir_path, sdb.get_path(), sizeof(notdir_path));
  LIBC_NAMESPACE::internal::strlcpy(notdir_path + BASE_LEN, "/services",
                                    sizeof(notdir_path) - BASE_LEN);
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
