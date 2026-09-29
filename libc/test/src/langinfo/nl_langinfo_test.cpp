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
#include "hdr/locale_macros.h"
#include "hdr/types/locale_t.h"
#include "src/__support/CPP/scope.h"
#include "src/langinfo/nl_langinfo.h"
#include "src/langinfo/nl_langinfo_l.h"
#include "src/locale/freelocale.h"
#include "src/locale/locale_data.h"
#include "src/locale/newlocale.h"
#include "src/locale/setlocale.h"
#include "src/locale/uselocale.h"
#include "src/stdlib/setenv.h"
#include "src/stdlib/unsetenv.h"
#include "test/UnitTest/Test.h"

TEST(LlvmLibcLanginfo, DefaultCLocaleItems) {
  constexpr const char *EXPECTED_CODESET =
      LIBC_NAMESPACE::DEFAULT_LOCALE_IS_UTF8 ? "UTF-8" : "US-ASCII";
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(CODESET), EXPECTED_CODESET);
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(RADIXCHAR), ".");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(THOUSEP), "");

  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(D_T_FMT), "%a %b %e %H:%M:%S %Y");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(D_FMT), "%m/%d/%y");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(T_FMT), "%H:%M:%S");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(T_FMT_AMPM), "%I:%M:%S %p");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(AM_STR), "AM");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(PM_STR), "PM");

  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(DAY_1), "Sunday");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(DAY_2), "Monday");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(DAY_3), "Tuesday");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(DAY_4), "Wednesday");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(DAY_5), "Thursday");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(DAY_6), "Friday");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(DAY_7), "Saturday");

  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(ABDAY_1), "Sun");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(ABDAY_2), "Mon");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(ABDAY_3), "Tue");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(ABDAY_4), "Wed");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(ABDAY_5), "Thu");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(ABDAY_6), "Fri");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(ABDAY_7), "Sat");

  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(MON_1), "January");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(MON_2), "February");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(MON_3), "March");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(MON_4), "April");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(MON_5), "May");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(MON_6), "June");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(MON_7), "July");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(MON_8), "August");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(MON_9), "September");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(MON_10), "October");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(MON_11), "November");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(MON_12), "December");

  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(ABMON_1), "Jan");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(ABMON_2), "Feb");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(ABMON_3), "Mar");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(ABMON_4), "Apr");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(ABMON_5), "May");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(ABMON_6), "Jun");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(ABMON_7), "Jul");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(ABMON_8), "Aug");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(ABMON_9), "Sep");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(ABMON_10), "Oct");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(ABMON_11), "Nov");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(ABMON_12), "Dec");

  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(ERA), "");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(ERA_D_FMT), "");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(ERA_D_T_FMT), "");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(ERA_T_FMT), "");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(ALT_DIGITS), "");

  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(CRNCYSTR), "");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(YESEXPR), "^[yY]");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(NOEXPR), "^[nN]");
}

TEST(LlvmLibcLanginfo, DefaultCLocaleItemsL) {
  locale_t loc = LIBC_NAMESPACE::newlocale(LC_ALL_MASK, "", nullptr);
  ASSERT_NE(loc, nullptr);
  LIBC_NAMESPACE::cpp::scope_exit free_loc(
      [&] { LIBC_NAMESPACE::freelocale(loc); });

  constexpr const char *EXPECTED_CODESET =
      LIBC_NAMESPACE::DEFAULT_LOCALE_IS_UTF8 ? "UTF-8" : "US-ASCII";
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo_l(CODESET, loc), EXPECTED_CODESET);
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo_l(RADIXCHAR, loc), ".");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo_l(DAY_2, loc), "Monday");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo_l(MON_2, loc), "February");
}

TEST(LlvmLibcLanginfo, InvalidItemReturnsEmptyString) {
  locale_t loc = LIBC_NAMESPACE::newlocale(LC_ALL_MASK, "C", nullptr);
  ASSERT_NE(loc, nullptr);
  LIBC_NAMESPACE::cpp::scope_exit free_loc(
      [&] { LIBC_NAMESPACE::freelocale(loc); });

  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(-1), "");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(9999999), "");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(_NL_ITEM(LC_CTYPE, 99)), "");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(_NL_ITEM(LC_NUMERIC, 99)), "");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(_NL_ITEM(LC_TIME, 99)), "");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(_NL_ITEM(LC_COLLATE, 0)), "");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(_NL_ITEM(LC_MONETARY, 99)), "");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(_NL_ITEM(LC_MESSAGES, 99)), "");
  EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo_l(-1, loc), "");
}

#if defined(LIBC_ADD_NULL_CHECKS)
TEST(LlvmLibcLanginfo, NullLocaleCrash) {
  ASSERT_DEATH([] { LIBC_NAMESPACE::nl_langinfo_l(CODESET, nullptr); },
               WITH_SIGNAL(-1));
}
#endif

TEST(LlvmLibcLanginfo, Utf8LocaleSupport) {
  if constexpr (!LIBC_NAMESPACE::DISABLE_RUNTIME_LOCALE) {
    constexpr const char *DEFAULT_NAME =
        LIBC_NAMESPACE::DEFAULT_LOCALE_IS_UTF8 ? "C.UTF-8" : "C";
    LIBC_NAMESPACE::cpp::scope_exit restore_ambient([&] {
      LIBC_NAMESPACE::unsetenv("LC_ALL");
      LIBC_NAMESPACE::setlocale(LC_ALL, DEFAULT_NAME);
    });

    locale_t c_loc = LIBC_NAMESPACE::newlocale(LC_ALL_MASK, "C", nullptr);
    ASSERT_NE(c_loc, nullptr);
    LIBC_NAMESPACE::cpp::scope_exit free_c(
        [&] { LIBC_NAMESPACE::freelocale(c_loc); });
    EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo_l(CODESET, c_loc), "US-ASCII");

    locale_t utf8_loc =
        LIBC_NAMESPACE::newlocale(LC_ALL_MASK, "C.utf-8", nullptr);
    ASSERT_NE(utf8_loc, nullptr);
    LIBC_NAMESPACE::cpp::scope_exit free_utf8(
        [&] { LIBC_NAMESPACE::freelocale(utf8_loc); });
    EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo_l(CODESET, utf8_loc), "UTF-8");
    EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo_l(RADIXCHAR, utf8_loc), ".");
    EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo_l(DAY_1, utf8_loc), "Sunday");

    // Test thread-local locale switching via uselocale.
    locale_t old_loc = LIBC_NAMESPACE::uselocale(c_loc);
    {
      LIBC_NAMESPACE::cpp::scope_exit restore_thread(
          [&] { LIBC_NAMESPACE::uselocale(old_loc); });
      EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(CODESET), "US-ASCII");
      LIBC_NAMESPACE::uselocale(utf8_loc);
      EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(CODESET), "UTF-8");
    }

    // Test global locale switching via setlocale.
    EXPECT_STREQ(LIBC_NAMESPACE::setlocale(LC_ALL, "C"), "C");
    EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(CODESET), "US-ASCII");
    EXPECT_STREQ(LIBC_NAMESPACE::setlocale(LC_ALL, "C.utf-8"), "C.UTF-8");
    EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(CODESET), "UTF-8");

    // Test environment-driven locale resolution via setlocale(LC_ALL, "").
    ASSERT_EQ(LIBC_NAMESPACE::setenv("LC_ALL", "C", 1), 0);
    EXPECT_STREQ(LIBC_NAMESPACE::setlocale(LC_ALL, ""), "C");
    EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(CODESET), "US-ASCII");

    ASSERT_EQ(LIBC_NAMESPACE::setenv("LC_ALL", "C.utf-8", 1), 0);
    EXPECT_STREQ(LIBC_NAMESPACE::setlocale(LC_ALL, ""), "C.UTF-8");
    EXPECT_STREQ(LIBC_NAMESPACE::nl_langinfo(CODESET), "UTF-8");
  }
}
