//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Unit tests for getservbyname_r, getservbyname, and getservbyport.
///
//===----------------------------------------------------------------------===//

#include "hdr/errno_macros.h"
#include "hdr/signal_macros.h"
#include "hdr/types/in_port_t.h"
#include "hdr/types/size_t.h"
#include "hdr/types/struct_servent.h"
#include "src/__support/net/byte_order.h"
#include "src/__support/netdb/services.h"
#include "src/netdb/getservbyname.h"
#include "src/netdb/getservbyname_r.h"
#include "src/netdb/getservbyport.h"
#include "test/UnitTest/ErrnoCheckingTest.h"
#include "test/UnitTest/Test.h"
#include "test/src/netdb/test_db_file.h"

class LlvmLibcGetservbynameTest
    : public LIBC_NAMESPACE::testing::ErrnoCheckingTest {
protected:
  void TearDown() override {
    LIBC_NAMESPACE::netdb::set_services_path(nullptr);
    ErrnoCheckingTest::TearDown();
  }
};

TEST_F(LlvmLibcGetservbynameTest, AliasCanonicalAndTlsWrappers) {
  ScopedTestDbFile db(libc_make_test_file_path("getservbyname.test"),
                      "# services\n"
                      "http 80/tcp www www-http\n"
                      "domain 53/udp dns\n"
                      "discard 9/sctp sink\n",
                      &LIBC_NAMESPACE::netdb::set_services_path);

  const int PORT_80 =
      static_cast<int>(LIBC_NAMESPACE::net::hton<in_port_t>(80));

  struct servent entry{};
  struct servent *result = nullptr;
  char buffer[128];

  ASSERT_EQ(LIBC_NAMESPACE::getservbyname_r("www", nullptr, &entry, buffer,
                                            sizeof(buffer), &result),
            0);
  ASSERT_EQ(result, &entry);
  EXPECT_STREQ(entry.s_name, "http");
  EXPECT_EQ(entry.s_port, PORT_80);
  EXPECT_STREQ(entry.s_proto, "tcp");
  ASSERT_NE(entry.s_aliases, nullptr);
  EXPECT_STREQ(entry.s_aliases[0], "www");
  EXPECT_STREQ(entry.s_aliases[1], "www-http");
  EXPECT_EQ(entry.s_aliases[2], nullptr);

  ASSERT_EQ(LIBC_NAMESPACE::getservbyname_r("www", "udp", &entry, buffer,
                                            sizeof(buffer), &result),
            0);
  EXPECT_EQ(result, nullptr);

  EXPECT_EQ(LIBC_NAMESPACE::getservbyname_r("www", "tcp", &entry, nullptr, 0,
                                            &result),
            ERANGE);
  EXPECT_EQ(result, nullptr);

  struct servent *tls_name = LIBC_NAMESPACE::getservbyname("sink", "sctp");
  ASSERT_NE(tls_name, nullptr);
  EXPECT_STREQ(tls_name->s_name, "discard");
  EXPECT_STREQ(tls_name->s_proto, "sctp");
  EXPECT_STREQ(tls_name->s_aliases[0], "sink");
  EXPECT_EQ(tls_name->s_aliases[1], nullptr);

  EXPECT_EQ(LIBC_NAMESPACE::getservbyname("nonexistent", nullptr), nullptr);

  struct servent *tls_port = LIBC_NAMESPACE::getservbyport(PORT_80, nullptr);
  ASSERT_NE(tls_port, nullptr);
  EXPECT_STREQ(tls_port->s_name, "http");
  EXPECT_EQ(tls_port->s_port, PORT_80);

  EXPECT_EQ(LIBC_NAMESPACE::getservbyport(1, "tcp"), nullptr);
}

#if defined(LIBC_ADD_NULL_CHECKS)
TEST_F(LlvmLibcGetservbynameTest, NullptrCrash) {
  struct servent entry{};
  struct servent *result = nullptr;
  char buffer[32];

  ASSERT_DEATH(
      [&] {
        LIBC_NAMESPACE::getservbyname_r(nullptr, nullptr, &entry, buffer,
                                        sizeof(buffer), &result);
      },
      WITH_SIGNAL(-1));
  ASSERT_DEATH(
      [&] {
        LIBC_NAMESPACE::getservbyname_r("http", nullptr, nullptr, buffer,
                                        sizeof(buffer), &result);
      },
      WITH_SIGNAL(-1));
  ASSERT_DEATH(
      [&] {
        LIBC_NAMESPACE::getservbyname_r("http", nullptr, &entry, nullptr,
                                        sizeof(buffer), &result);
      },
      WITH_SIGNAL(-1));
  ASSERT_DEATH(
      [&] {
        LIBC_NAMESPACE::getservbyname_r("http", nullptr, &entry, buffer,
                                        sizeof(buffer), nullptr);
      },
      WITH_SIGNAL(-1));
  ASSERT_DEATH([] { LIBC_NAMESPACE::getservbyname(nullptr, nullptr); },
               WITH_SIGNAL(-1));
}
#endif // LIBC_ADD_NULL_CHECKS
