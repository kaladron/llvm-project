//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Unit tests for getprotobyname and getprotobynumber.
///
//===----------------------------------------------------------------------===//

#include "hdr/signal_macros.h"
#include "hdr/types/struct_protoent.h"
#include "src/netdb/getproto_internal.h"
#include "src/netdb/getprotobyname.h"
#include "src/netdb/getprotobynumber.h"
#include "test/UnitTest/ErrnoCheckingTest.h"
#include "test/UnitTest/Test.h"
#include "test/src/netdb/test_db_file.h"

using LlvmLibcGetprotobynameTest = LIBC_NAMESPACE::testing::ErrnoCheckingTest;

TEST_F(LlvmLibcGetprotobynameTest, NameAliasAndNumberLookups) {
  ScopedTestDbFile db(libc_make_test_file_path("getprotobyname_tls.test"),
                      "# protocols\n"
                      "icmp\t1\tICMP\n"
                      "udp\t17\tUDP dgram\n",
                      &LIBC_NAMESPACE::netdb::set_protocols_path);

  struct protoent *by_alias = LIBC_NAMESPACE::getprotobyname("dgram");
  ASSERT_NE(by_alias, nullptr);
  EXPECT_STREQ(by_alias->p_name, "udp");
  EXPECT_EQ(by_alias->p_proto, 17);
  ASSERT_NE(by_alias->p_aliases, nullptr);
  EXPECT_STREQ(by_alias->p_aliases[0], "UDP");
  EXPECT_STREQ(by_alias->p_aliases[1], "dgram");
  EXPECT_EQ(by_alias->p_aliases[2], nullptr);

  EXPECT_EQ(LIBC_NAMESPACE::getprotobyname("missing"), nullptr);

  struct protoent *by_num = LIBC_NAMESPACE::getprotobynumber(1);
  ASSERT_NE(by_num, nullptr);
  EXPECT_STREQ(by_num->p_name, "icmp");
  EXPECT_EQ(by_num->p_proto, 1);
  ASSERT_NE(by_num->p_aliases, nullptr);
  EXPECT_STREQ(by_num->p_aliases[0], "ICMP");
  EXPECT_EQ(by_num->p_aliases[1], nullptr);

  EXPECT_EQ(LIBC_NAMESPACE::getprotobynumber(255), nullptr);
}

#if defined(LIBC_ADD_NULL_CHECKS)
TEST_F(LlvmLibcGetprotobynameTest, NullptrCrash) {
  ASSERT_DEATH([] { LIBC_NAMESPACE::getprotobyname(nullptr); },
               WITH_SIGNAL(-1));
}
#endif // LIBC_ADD_NULL_CHECKS
