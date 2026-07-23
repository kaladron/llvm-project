//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Unittests for nl_langinfo and nl_langinfo_l.
///
//===----------------------------------------------------------------------===//

#include "hdr/langinfo_macros.h"
#include "src/langinfo/nl_langinfo.h"
#include "src/langinfo/nl_langinfo_l.h"
#include "test/UnitTest/Test.h"

TEST(LlvmLibcLanginfo, DefaultCLocaleItems) {
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(CODESET), "US-ASCII");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(RADIXCHAR), ".");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(THOUSEP), "");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(DAY_1), "Sunday");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(DAY_7), "Saturday");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(ABDAY_1), "Sun");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(MON_1), "January");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(MON_12), "December");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(ABMON_1), "Jan");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(YESEXPR), "^[yY]");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(NOEXPR), "^[nN]");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(CRNCYSTR), "");
}

TEST(LlvmLibcLanginfo, DefaultCLocaleItemsL) {
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo_l(CODESET, nullptr), "US-ASCII");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo_l(RADIXCHAR, nullptr), ".");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo_l(DAY_2, nullptr), "Monday");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo_l(MON_2, nullptr), "February");
}

TEST(LlvmLibcLanginfo, InvalidItemReturnsEmptyString) {
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(-1), "");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(9999999), "");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo_l(-1, nullptr), "");
}
