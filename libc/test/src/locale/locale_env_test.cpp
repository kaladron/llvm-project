//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Unittests for environment-driven locale resolution.
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
#include "src/stdlib/setenv.h"
#include "src/stdlib/unsetenv.h"
#include "test/UnitTest/ErrnoCheckingTest.h"
#include "test/UnitTest/Test.h"

using LlvmLibcLocaleEnv = LIBC_NAMESPACE::testing::ErrnoCheckingTest;

static void clear_locale_env() {
  constexpr const char *LOCALE_VARS[] = {
      "LC_ALL",     "LC_CTYPE",    "LC_NUMERIC",  "LC_TIME",
      "LC_COLLATE", "LC_MONETARY", "LC_MESSAGES", "LANG"};
  for (const char *var : LOCALE_VARS)
    LIBC_NAMESPACE::unsetenv(var);
}

TEST_F(LlvmLibcLocaleEnv, EnvironmentLocale) {
  constexpr const char *DEFAULT_NAME =
      LIBC_NAMESPACE::DEFAULT_LOCALE_IS_UTF8 ? "C.UTF-8" : "C";
  LIBC_NAMESPACE::cpp::scope_exit restore_env([&] {
    clear_locale_env();
    LIBC_NAMESPACE::setlocale(LC_ALL, DEFAULT_NAME);
  });

  clear_locale_env();

  // Unset environment falls back to the compiled-in default locale.
  EXPECT_STREQ(LIBC_NAMESPACE::setlocale(LC_ALL, ""), DEFAULT_NAME);
  locale_t loc = LIBC_NAMESPACE::newlocale(LC_ALL_MASK, "", nullptr);
  ASSERT_NE(loc, nullptr);
  LIBC_NAMESPACE::freelocale(loc);

  if constexpr (LIBC_NAMESPACE::ENABLE_RUNTIME_LOCALE) {
    // LANG=C.utf-8 selects UTF-8.
    ASSERT_EQ(LIBC_NAMESPACE::setenv("LANG", "C.utf-8", 1), 0);
    EXPECT_STREQ(LIBC_NAMESPACE::setlocale(LC_ALL, ""), "C.UTF-8");

    // LC_CTYPE=C overrides LANG=C.utf-8.
    ASSERT_EQ(LIBC_NAMESPACE::setenv("LC_CTYPE", "C", 1), 0);
    EXPECT_STREQ(LIBC_NAMESPACE::setlocale(LC_ALL, ""), "C");

    // LC_ALL=en_US.UTF-8 overrides LC_CTYPE=C and LANG.
    ASSERT_EQ(LIBC_NAMESPACE::setenv("LC_ALL", "en_US.UTF-8", 1), 0);
    EXPECT_STREQ(LIBC_NAMESPACE::setlocale(LC_ALL, ""), "C.UTF-8");

    // Unsupported environment locale fails cleanly.
    ASSERT_EQ(LIBC_NAMESPACE::setenv("LC_ALL", "does-not-exist", 1), 0);
    EXPECT_EQ(LIBC_NAMESPACE::setlocale(LC_ALL, ""), nullptr);
    EXPECT_EQ(LIBC_NAMESPACE::newlocale(LC_ALL_MASK, "", nullptr), nullptr);
    ASSERT_ERRNO_EQ(ENOENT);
  }
}
