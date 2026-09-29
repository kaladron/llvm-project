//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Unittests for nl_langinfo and nl_langinfo_l with runtime locale switching.
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

static void clear_locale_env() {
  constexpr const char *LOCALE_VARS[] = {
      "LC_ALL",     "LC_CTYPE",    "LC_NUMERIC",  "LC_TIME",
      "LC_COLLATE", "LC_MONETARY", "LC_MESSAGES", "LANG"};
  for (const char *var : LOCALE_VARS)
    LIBC_NAMESPACE::unsetenv(var);
}

TEST(LlvmLibcLanginfoLocale, Utf8LocaleSupport) {
  if constexpr (LIBC_NAMESPACE::ENABLE_RUNTIME_LOCALE) {
    constexpr const char *DEFAULT_NAME =
        LIBC_NAMESPACE::DEFAULT_LOCALE_IS_UTF8 ? "C.UTF-8" : "C";
    LIBC_NAMESPACE::cpp::scope_exit restore_ambient([&] {
      clear_locale_env();
      LIBC_NAMESPACE::setlocale(LC_ALL, DEFAULT_NAME);
    });

    clear_locale_env();

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
