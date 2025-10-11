//===-- Unittests for putenv ----------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/stdlib/getenv.h"
#include "src/stdlib/putenv.h"
#include "src/stdlib/unsetenv.h"
#include "src/string/strcmp.h"
#include "src/string/strcpy.h"

#include "test/IntegrationTest/test.h"

#include <errno.h>

TEST_MAIN([[maybe_unused]] int argc, [[maybe_unused]] char **argv,
          [[maybe_unused]] char **envp) {
  // Test: Basic
  {
    // putenv uses the caller's buffer directly, so it must remain valid
    static char env_string[] = "PUTENV_TEST_VAR=test_value";

    ASSERT_EQ(LIBC_NAMESPACE::putenv(env_string), 0);

    char *value = LIBC_NAMESPACE::getenv("PUTENV_TEST_VAR");
    ASSERT_TRUE(value != nullptr);
    ASSERT_EQ(LIBC_NAMESPACE::strcmp(value, "test_value"), 0);

    // Clean up
    LIBC_NAMESPACE::unsetenv("PUTENV_TEST_VAR");
  }

  // Test: NullString
  {
    errno = 0;
    ASSERT_EQ(LIBC_NAMESPACE::putenv(nullptr), -1);
    ASSERT_ERRNO_EQ(EINVAL);
  }

  // Test: NoEquals
  {
    // String must contain '='
    static char env_string[] = "NO_EQUALS_SIGN";

    errno = 0;
    ASSERT_EQ(LIBC_NAMESPACE::putenv(env_string), -1);
    ASSERT_ERRNO_EQ(EINVAL);
  }

  // Test: EmptyName
  {
    // Name before '=' cannot be empty
    static char env_string[] = "=value";

    errno = 0;
    ASSERT_EQ(LIBC_NAMESPACE::putenv(env_string), -1);
    ASSERT_ERRNO_EQ(EINVAL);
  }

  // Test: EmptyValue
  {
    // Empty value after '=' is valid
    static char env_string[] = "EMPTY_VALUE=";

    ASSERT_EQ(LIBC_NAMESPACE::putenv(env_string), 0);

    char *value = LIBC_NAMESPACE::getenv("EMPTY_VALUE");
    ASSERT_TRUE(value != nullptr);
    ASSERT_EQ(LIBC_NAMESPACE::strcmp(value, ""), 0);

    // Clean up
    LIBC_NAMESPACE::unsetenv("EMPTY_VALUE");
  }

  // Test: CallerOwnsMemory
  {
    // putenv does NOT copy the string - it uses the caller's pointer
    // So modifying the string should affect the environment
    static char env_string[] = "OWNED_VAR=initial_value";

    ASSERT_EQ(LIBC_NAMESPACE::putenv(env_string), 0);
    ASSERT_EQ(LIBC_NAMESPACE::strcmp(LIBC_NAMESPACE::getenv("OWNED_VAR"),
                                     "initial_value"),
              0);

    // Modify the string directly
    LIBC_NAMESPACE::strcpy(env_string + 10, "modified_value");

    // The environment should see the change
    ASSERT_EQ(LIBC_NAMESPACE::strcmp(LIBC_NAMESPACE::getenv("OWNED_VAR"),
                                     "modified_value"),
              0);

    // Clean up
    LIBC_NAMESPACE::unsetenv("OWNED_VAR");
  }

  // Test: ReplaceExisting
  {
    // First putenv
    static char env_string1[] = "REPLACE_VAR=first";
    ASSERT_EQ(LIBC_NAMESPACE::putenv(env_string1), 0);
    ASSERT_EQ(
        LIBC_NAMESPACE::strcmp(LIBC_NAMESPACE::getenv("REPLACE_VAR"), "first"),
        0);

    // Second putenv with same variable name
    static char env_string2[] = "REPLACE_VAR=second";
    ASSERT_EQ(LIBC_NAMESPACE::putenv(env_string2), 0);
    ASSERT_EQ(
        LIBC_NAMESPACE::strcmp(LIBC_NAMESPACE::getenv("REPLACE_VAR"), "second"),
        0);

    // Clean up
    LIBC_NAMESPACE::unsetenv("REPLACE_VAR");
  }

  // Test: MultipleVariables
  {
    static char env1[] = "PUTENV_VAR1=value1";
    static char env2[] = "PUTENV_VAR2=value2";
    static char env3[] = "PUTENV_VAR3=value3";

    ASSERT_EQ(LIBC_NAMESPACE::putenv(env1), 0);
    ASSERT_EQ(LIBC_NAMESPACE::putenv(env2), 0);
    ASSERT_EQ(LIBC_NAMESPACE::putenv(env3), 0);

    ASSERT_EQ(
        LIBC_NAMESPACE::strcmp(LIBC_NAMESPACE::getenv("PUTENV_VAR1"), "value1"),
        0);
    ASSERT_EQ(
        LIBC_NAMESPACE::strcmp(LIBC_NAMESPACE::getenv("PUTENV_VAR2"), "value2"),
        0);
    ASSERT_EQ(
        LIBC_NAMESPACE::strcmp(LIBC_NAMESPACE::getenv("PUTENV_VAR3"), "value3"),
        0);

    // Clean up
    LIBC_NAMESPACE::unsetenv("PUTENV_VAR1");
    LIBC_NAMESPACE::unsetenv("PUTENV_VAR2");
    LIBC_NAMESPACE::unsetenv("PUTENV_VAR3");
  }

  // Test: EqualsInValue
  {
    // Value part can contain '=' characters
    static char env_string[] = "EQUALS_VAR=value=with=equals";

    ASSERT_EQ(LIBC_NAMESPACE::putenv(env_string), 0);
    ASSERT_EQ(LIBC_NAMESPACE::strcmp(LIBC_NAMESPACE::getenv("EQUALS_VAR"),
                                     "value=with=equals"),
              0);

    // Clean up
    LIBC_NAMESPACE::unsetenv("EQUALS_VAR");
  }

  // Test: SpecialCharactersInValue
  {
    static char env_string[] = "SPECIAL_VAR=!@#$%^&*()";

    ASSERT_EQ(LIBC_NAMESPACE::putenv(env_string), 0);
    ASSERT_EQ(LIBC_NAMESPACE::strcmp(LIBC_NAMESPACE::getenv("SPECIAL_VAR"),
                                     "!@#$%^&*()"),
              0);

    // Clean up
    LIBC_NAMESPACE::unsetenv("SPECIAL_VAR");
  }

  // Test: LongString
  {
    static char env_string[] =
        "LONG_PUTENV_VAR=This is a very long value string to test that "
        "putenv handles long strings correctly without any issues";

    ASSERT_EQ(LIBC_NAMESPACE::putenv(env_string), 0);

    char *value = LIBC_NAMESPACE::getenv("LONG_PUTENV_VAR");
    ASSERT_TRUE(value != nullptr);
    ASSERT_EQ(LIBC_NAMESPACE::strcmp(
                  value,
                  "This is a very long value string to test that "
                  "putenv handles long strings correctly without any issues"),
              0);

    // Clean up
    LIBC_NAMESPACE::unsetenv("LONG_PUTENV_VAR");
  }

  return 0;
}
