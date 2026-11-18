//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Unit tests for getnetbyaddr_r.
///
//===----------------------------------------------------------------------===//

#include "hdr/errno_macros.h"
#include "hdr/netdb_macros.h"
#include "hdr/signal_macros.h"
#include "hdr/stdint_proxy.h"
#include "hdr/sys_socket_macros.h"
#include "hdr/types/size_t.h"
#include "hdr/types/struct_netent.h"
#include "src/netdb/getnet_internal.h"
#include "src/netdb/getnetbyaddr_r.h"
#include "test/UnitTest/ErrnoCheckingTest.h"
#include "test/UnitTest/Test.h"
#include "test/src/netdb/test_db_file.h"

class LlvmLibcGetnetbyaddrRTest
    : public LIBC_NAMESPACE::testing::ErrnoCheckingTest {
protected:
  void TearDown() override {
    LIBC_NAMESPACE::netdb::set_networks_path(nullptr);
    ErrnoCheckingTest::TearDown();
  }
};

TEST_F(LlvmLibcGetnetbyaddrRTest, HostOrderLookupsAndFamilyFilter) {
  ScopedTestDbFile db(libc_make_test_file_path("getnetbyaddr_r.test"),
                      "# networks\n"
                      "loopback 127.0.0.0 lo\n"
                      "corp 10.1.2 corp-net\n"
                      "class_a 127\n",
                      &LIBC_NAMESPACE::netdb::set_networks_path);

  struct netent entry{};
  struct netent *result = nullptr;
  char buffer[128];
  int herr = 0;

  ASSERT_EQ(LIBC_NAMESPACE::getnetbyaddr_r(0x7f000000U, AF_INET, &entry, buffer,
                                           sizeof(buffer), &result, &herr),
            0);
  ASSERT_EQ(result, &entry);
  EXPECT_STREQ(entry.n_name, "loopback");
  EXPECT_EQ(entry.n_addrtype, AF_INET);
  EXPECT_EQ(entry.n_net, uint32_t(0x7f000000));
  ASSERT_NE(entry.n_aliases, nullptr);
  EXPECT_STREQ(entry.n_aliases[0], "lo");
  EXPECT_EQ(entry.n_aliases[1], nullptr);

  ASSERT_EQ(LIBC_NAMESPACE::getnetbyaddr_r(0x000a0102U, AF_INET, &entry, buffer,
                                           sizeof(buffer), &result, &herr),
            0);
  ASSERT_EQ(result, &entry);
  EXPECT_STREQ(entry.n_name, "corp");

  ASSERT_EQ(LIBC_NAMESPACE::getnetbyaddr_r(127U, AF_INET, &entry, buffer,
                                           sizeof(buffer), &result, &herr),
            0);
  ASSERT_EQ(result, &entry);
  EXPECT_STREQ(entry.n_name, "class_a");

  herr = 0;
  result = &entry;
  ASSERT_EQ(LIBC_NAMESPACE::getnetbyaddr_r(0x7f000000U, AF_INET6, &entry,
                                           buffer, sizeof(buffer), &result,
                                           &herr),
            0);
  EXPECT_EQ(result, nullptr);
  EXPECT_EQ(herr, HOST_NOT_FOUND);

  herr = 0;
  result = &entry;
  ASSERT_EQ(LIBC_NAMESPACE::getnetbyaddr_r(0xdeadbeefU, AF_INET, &entry, buffer,
                                           sizeof(buffer), &result, &herr),
            0);
  EXPECT_EQ(result, nullptr);
  EXPECT_EQ(herr, HOST_NOT_FOUND);

  herr = 0;
  result = &entry;
  EXPECT_EQ(LIBC_NAMESPACE::getnetbyaddr_r(0x7f000000U, AF_INET, &entry,
                                           nullptr, 0, &result, &herr),
            ERANGE);
  EXPECT_EQ(result, nullptr);
  EXPECT_EQ(herr, NO_RECOVERY);
}

#if defined(LIBC_ADD_NULL_CHECKS)
TEST_F(LlvmLibcGetnetbyaddrRTest, NullptrCrash) {
  struct netent entry{};
  struct netent *result = nullptr;
  char buffer[32];
  int herr = 0;

  ASSERT_DEATH(
      [&] {
        LIBC_NAMESPACE::getnetbyaddr_r(127U, AF_INET, nullptr, buffer,
                                       sizeof(buffer), &result, &herr);
      },
      WITH_SIGNAL(-1));
  ASSERT_DEATH(
      [&] {
        LIBC_NAMESPACE::getnetbyaddr_r(127U, AF_INET, &entry, nullptr,
                                       sizeof(buffer), &result, &herr);
      },
      WITH_SIGNAL(-1));
  ASSERT_DEATH(
      [&] {
        LIBC_NAMESPACE::getnetbyaddr_r(127U, AF_INET, &entry, buffer,
                                       sizeof(buffer), nullptr, &herr);
      },
      WITH_SIGNAL(-1));
  ASSERT_DEATH(
      [&] {
        LIBC_NAMESPACE::getnetbyaddr_r(127U, AF_INET, &entry, buffer,
                                       sizeof(buffer), &result, nullptr);
      },
      WITH_SIGNAL(-1));
}
#endif // LIBC_ADD_NULL_CHECKS
