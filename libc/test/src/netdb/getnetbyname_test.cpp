//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Unit tests for getnetbyname and getnetbyaddr.
///
//===----------------------------------------------------------------------===//

#include "hdr/netdb_macros.h"
#include "hdr/signal_macros.h"
#include "hdr/stdint_proxy.h"
#include "hdr/sys_socket_macros.h"
#include "hdr/types/struct_netent.h"
#include "src/netdb/__h_errno_location.h"
#include "src/netdb/getnet_internal.h"
#include "src/netdb/getnetbyaddr.h"
#include "src/netdb/getnetbyname.h"
#include "test/UnitTest/ErrnoCheckingTest.h"
#include "test/UnitTest/Test.h"
#include "test/src/netdb/test_db_file.h"

using LlvmLibcGetnetbynameTest = LIBC_NAMESPACE::testing::ErrnoCheckingTest;

TEST_F(LlvmLibcGetnetbynameTest, NameAliasAndAddrLookupsPreserveHErrno) {
  ScopedTestDbFile db(libc_make_test_file_path("getnetbyname_tls.test"),
                      "# networks\n"
                      "loopback 127.0.0.0 lo\n"
                      "campus 172.16 campus-net\n",
                      &LIBC_NAMESPACE::netdb::set_networks_path);

  *LIBC_NAMESPACE::__h_errno_location() = 0;

  struct netent *by_alias = LIBC_NAMESPACE::getnetbyname("campus-net");
  ASSERT_NE(by_alias, nullptr);
  EXPECT_STREQ(by_alias->n_name, "campus");
  EXPECT_EQ(by_alias->n_addrtype, AF_INET);
  EXPECT_EQ(by_alias->n_net, uint32_t(0x0000ac10));
  ASSERT_NE(by_alias->n_aliases, nullptr);
  EXPECT_STREQ(by_alias->n_aliases[0], "campus-net");
  EXPECT_EQ(by_alias->n_aliases[1], nullptr);

  EXPECT_EQ(LIBC_NAMESPACE::getnetbyname("missing"), nullptr);
  EXPECT_EQ(*LIBC_NAMESPACE::__h_errno_location(), 0);

  struct netent *by_addr = LIBC_NAMESPACE::getnetbyaddr(0x7f000000U, AF_INET);
  ASSERT_NE(by_addr, nullptr);
  EXPECT_STREQ(by_addr->n_name, "loopback");
  EXPECT_EQ(by_addr->n_addrtype, AF_INET);
  EXPECT_EQ(by_addr->n_net, uint32_t(0x7f000000));

  EXPECT_EQ(LIBC_NAMESPACE::getnetbyaddr(0x7f000000U, AF_INET6), nullptr);
  EXPECT_EQ(LIBC_NAMESPACE::getnetbyaddr(0xdeadbeefU, AF_INET), nullptr);
  EXPECT_EQ(*LIBC_NAMESPACE::__h_errno_location(), 0);
}

#if defined(LIBC_ADD_NULL_CHECKS)
TEST_F(LlvmLibcGetnetbynameTest, NullptrCrash) {
  ASSERT_DEATH([] { LIBC_NAMESPACE::getnetbyname(nullptr); }, WITH_SIGNAL(-1));
}
#endif // LIBC_ADD_NULL_CHECKS
