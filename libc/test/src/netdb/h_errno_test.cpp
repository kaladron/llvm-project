//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Unit tests for __h_errno_location and netdb error macros.
///
//===----------------------------------------------------------------------===//

#include "hdr/netdb_macros.h"
#include "src/netdb/__h_errno_location.h"
#include "test/UnitTest/Test.h"

TEST(LlvmLibcHErrnoTest, MacrosAndThreadLocalStorage) {
  EXPECT_EQ(IPPORT_RESERVED, 1024);
  EXPECT_EQ(HOST_NOT_FOUND, 1);
  EXPECT_EQ(TRY_AGAIN, 2);
  EXPECT_EQ(NO_RECOVERY, 3);
  EXPECT_EQ(NO_DATA, 4);
  EXPECT_EQ(NO_ADDRESS, NO_DATA);

  int *loc = LIBC_NAMESPACE::__h_errno_location();
  ASSERT_NE(loc, nullptr);
  EXPECT_EQ(*loc, 0);

  *loc = HOST_NOT_FOUND;
  EXPECT_EQ(*LIBC_NAMESPACE::__h_errno_location(), HOST_NOT_FOUND);

  *loc = 0;
  EXPECT_EQ(*LIBC_NAMESPACE::__h_errno_location(), 0);
}
