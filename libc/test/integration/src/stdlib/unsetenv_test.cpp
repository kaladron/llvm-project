//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/stdlib/getenv.h"
#include "src/stdlib/setenv.h"
#include "src/stdlib/unsetenv.h"
#include "src/string/strcmp.h"

#include "test/IntegrationTest/test.h"

#include <errno.h>

TEST_MAIN([[maybe_unused]] int argc, [[maybe_unused]] char **argv,
          [[maybe_unused]] char **envp) {
  // Test: Remove a variable set by setenv
  {
    ASSERT_EQ(LIBC_NAMESPACE::setenv("UNSET_VAR", "value", 1), 0);
    ASSERT_TRUE(LIBC_NAMESPACE::getenv("UNSET_VAR") != nullptr);

    ASSERT_EQ(LIBC_NAMESPACE::unsetenv("UNSET_VAR"), 0);
    ASSERT_TRUE(LIBC_NAMESPACE::getenv("UNSET_VAR") == nullptr);
  }

  // Test: Unset non-existent variable succeeds
  {
    ASSERT_EQ(LIBC_NAMESPACE::unsetenv("DOES_NOT_EXIST"), 0);
  }

  // Test: Empty name returns EINVAL
  {
    errno = 0;
    ASSERT_EQ(LIBC_NAMESPACE::unsetenv(""), -1);
    ASSERT_ERRNO_EQ(EINVAL);
  }

  // Test: Name with '=' returns EINVAL
  {
    errno = 0;
    ASSERT_EQ(LIBC_NAMESPACE::unsetenv("BAD=NAME"), -1);
    ASSERT_ERRNO_EQ(EINVAL);
  }

  // Test: Unset then re-set
  {
    ASSERT_EQ(LIBC_NAMESPACE::setenv("REUSE_VAR", "first", 1), 0);
    ASSERT_EQ(
        LIBC_NAMESPACE::strcmp(LIBC_NAMESPACE::getenv("REUSE_VAR"), "first"),
        0);

    ASSERT_EQ(LIBC_NAMESPACE::unsetenv("REUSE_VAR"), 0);
    ASSERT_TRUE(LIBC_NAMESPACE::getenv("REUSE_VAR") == nullptr);

    ASSERT_EQ(LIBC_NAMESPACE::setenv("REUSE_VAR", "second", 1), 0);
    ASSERT_EQ(
        LIBC_NAMESPACE::strcmp(LIBC_NAMESPACE::getenv("REUSE_VAR"), "second"),
        0);
  }

  // Test: Unset multiple variables
  {
    ASSERT_EQ(LIBC_NAMESPACE::setenv("MULTI_A", "a", 1), 0);
    ASSERT_EQ(LIBC_NAMESPACE::setenv("MULTI_B", "b", 1), 0);
    ASSERT_EQ(LIBC_NAMESPACE::setenv("MULTI_C", "c", 1), 0);

    ASSERT_EQ(LIBC_NAMESPACE::unsetenv("MULTI_B"), 0);

    ASSERT_TRUE(LIBC_NAMESPACE::getenv("MULTI_A") != nullptr);
    ASSERT_TRUE(LIBC_NAMESPACE::getenv("MULTI_B") == nullptr);
    ASSERT_TRUE(LIBC_NAMESPACE::getenv("MULTI_C") != nullptr);

    ASSERT_EQ(LIBC_NAMESPACE::strcmp(LIBC_NAMESPACE::getenv("MULTI_A"), "a"),
              0);
    ASSERT_EQ(LIBC_NAMESPACE::strcmp(LIBC_NAMESPACE::getenv("MULTI_C"), "c"),
              0);
  }

  // Test: Unset same variable twice is harmless
  {
    ASSERT_EQ(LIBC_NAMESPACE::setenv("DOUBLE_UNSET", "val", 1), 0);
    ASSERT_EQ(LIBC_NAMESPACE::unsetenv("DOUBLE_UNSET"), 0);
    ASSERT_EQ(LIBC_NAMESPACE::unsetenv("DOUBLE_UNSET"), 0);
    ASSERT_TRUE(LIBC_NAMESPACE::getenv("DOUBLE_UNSET") == nullptr);
  }

  return 0;
}
