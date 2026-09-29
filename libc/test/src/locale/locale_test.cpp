//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Unittests for locale functions.
///
//===----------------------------------------------------------------------===//

#include "hdr/errno_macros.h"
#include "hdr/locale_macros.h"
#include "hdr/types/locale_t.h"
#include "src/__support/CPP/scope.h"
#include "src/locale/freelocale.h"
#include "src/locale/locale_data.h"
#include "src/locale/newlocale.h"
#include "src/locale/setlocale.h"
#include "src/locale/uselocale.h"
#include "test/UnitTest/ErrnoCheckingTest.h"
#include "test/UnitTest/Test.h"

using LlvmLibcLocale = LIBC_NAMESPACE::testing::ErrnoCheckingTest;

TEST_F(LlvmLibcLocale, DefaultLocale) {
  locale_t new_locale = LIBC_NAMESPACE::newlocale(LC_ALL_MASK, "C", nullptr);
  ASSERT_NE(new_locale, nullptr);
  LIBC_NAMESPACE::cpp::scope_exit free_new(
      [&] { LIBC_NAMESPACE::freelocale(new_locale); });

  locale_t old_locale = LIBC_NAMESPACE::uselocale(new_locale);
  ASSERT_NE(old_locale, nullptr);
  EXPECT_NE(LIBC_NAMESPACE::uselocale(nullptr), nullptr);

  locale_t restored_locale = LIBC_NAMESPACE::uselocale(old_locale);
  EXPECT_NE(restored_locale, nullptr);
}

TEST_F(LlvmLibcLocale, NewLocaleValidation) {
  locale_t loc =
      LIBC_NAMESPACE::newlocale(LC_CTYPE_MASK | LC_NUMERIC_MASK, "C", nullptr);
  ASSERT_NE(loc, nullptr);
  LIBC_NAMESPACE::freelocale(loc);

  loc = LIBC_NAMESPACE::newlocale(LC_ALL_MASK, "POSIX", nullptr);
  ASSERT_NE(loc, nullptr);
  LIBC_NAMESPACE::freelocale(loc);

  if constexpr (LIBC_NAMESPACE::ENABLE_RUNTIME_LOCALE) {
    constexpr const char *UTF8_NAMES[] = {"C.UTF-8", "C.utf-8", "C.utf8",
                                          "en_US.UTF-8", "en_US.utf-8"};
    for (const char *utf8_name : UTF8_NAMES) {
      loc = LIBC_NAMESPACE::newlocale(LC_ALL_MASK, utf8_name, nullptr);
      ASSERT_NE(loc, nullptr);
      LIBC_NAMESPACE::freelocale(loc);
    }

    // Modifying a non-LC_CTYPE category preserves base's LC_CTYPE.
    locale_t utf8_base =
        LIBC_NAMESPACE::newlocale(LC_ALL_MASK, "C.UTF-8", nullptr);
    ASSERT_NE(utf8_base, nullptr);
    locale_t preserved =
        LIBC_NAMESPACE::newlocale(LC_TIME_MASK, "C", utf8_base);
    EXPECT_EQ(preserved, utf8_base);
    LIBC_NAMESPACE::freelocale(preserved);
  } else {
    loc = LIBC_NAMESPACE::newlocale(LC_ALL_MASK, "", nullptr);
    ASSERT_NE(loc, nullptr);
    LIBC_NAMESPACE::freelocale(loc);
  }

  EXPECT_EQ(LIBC_NAMESPACE::newlocale(~0, "C", nullptr), nullptr);
  ASSERT_ERRNO_EQ(EINVAL);

  EXPECT_EQ(LIBC_NAMESPACE::newlocale(LC_ALL_MASK, nullptr, nullptr), nullptr);
  ASSERT_ERRNO_EQ(EINVAL);

  EXPECT_EQ(LIBC_NAMESPACE::newlocale(LC_ALL_MASK, ".UTF-8", nullptr), nullptr);
  ASSERT_ERRNO_EQ(ENOENT);

  EXPECT_EQ(LIBC_NAMESPACE::newlocale(LC_ALL_MASK, "blah.UTF-8", nullptr),
            nullptr);
  ASSERT_ERRNO_EQ(ENOENT);

  EXPECT_EQ(LIBC_NAMESPACE::newlocale(LC_ALL_MASK, "does-not-exist", nullptr),
            nullptr);
  ASSERT_ERRNO_EQ(ENOENT);
}

TEST_F(LlvmLibcLocale, SetLocale) {
  constexpr const char *DEFAULT_NAME =
      LIBC_NAMESPACE::DEFAULT_LOCALE_IS_UTF8 ? "C.UTF-8" : "C";
  LIBC_NAMESPACE::cpp::scope_exit restore_locale(
      [&] { LIBC_NAMESPACE::setlocale(LC_ALL, DEFAULT_NAME); });

  EXPECT_EQ(LIBC_NAMESPACE::setlocale(-1, "C"), nullptr);
  EXPECT_EQ(LIBC_NAMESPACE::setlocale(LC_ALL + 1, "C"), nullptr);
  EXPECT_EQ(LIBC_NAMESPACE::setlocale(LC_ALL + 1, nullptr), nullptr);
  EXPECT_EQ(LIBC_NAMESPACE::setlocale(LC_ALL, "does-not-exist"), nullptr);

  EXPECT_STREQ(LIBC_NAMESPACE::setlocale(LC_ALL, "C"), "C");
  EXPECT_STREQ(LIBC_NAMESPACE::setlocale(LC_ALL, "POSIX"), "C");
  EXPECT_STREQ(LIBC_NAMESPACE::setlocale(LC_NUMERIC, nullptr), "C");

  if constexpr (LIBC_NAMESPACE::ENABLE_RUNTIME_LOCALE) {
    EXPECT_STREQ(LIBC_NAMESPACE::setlocale(LC_ALL, nullptr), "C");
    EXPECT_STREQ(LIBC_NAMESPACE::setlocale(LC_ALL, "C.utf-8"), "C.UTF-8");
    EXPECT_STREQ(LIBC_NAMESPACE::setlocale(LC_ALL, nullptr), "C.UTF-8");
    EXPECT_STREQ(LIBC_NAMESPACE::setlocale(LC_NUMERIC, nullptr), "C");
    EXPECT_STREQ(LIBC_NAMESPACE::setlocale(LC_ALL, "en_US.UTF-8"), "C.UTF-8");

    // Failed setlocale must leave global_locale unchanged.
    EXPECT_EQ(LIBC_NAMESPACE::setlocale(LC_ALL, "does-not-exist"), nullptr);
    EXPECT_STREQ(LIBC_NAMESPACE::setlocale(LC_ALL, nullptr), "C.UTF-8");
  }
}
