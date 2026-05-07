//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

/// \file
/// Integration tests for putenv.

#include "src/stdlib/getenv.h"
#include "src/stdlib/putenv.h"
#include "src/string/strcmp.h"
#include "src/unistd/environ.h"

#include "test/IntegrationTest/test.h"

namespace LIBC_NAMESPACE {

static char set_var[] = "PUTENV_TEST=hello";
static char replace_var[] = "PUTENV_TEST=world";
static char empty_value[] = "PUTENV_EMPTY=";
static char special_chars[] = "PUTENV_SPECIAL=!@#$%^&*()";

TEST_MAIN([[maybe_unused]] int argc, [[maybe_unused]] char **argv,
          [[maybe_unused]] char **envp) {
  // Test: Basic set
  {
    ASSERT_EQ(putenv(set_var), 0);
    char *value = getenv("PUTENV_TEST");
    ASSERT_TRUE(value != nullptr);
    ASSERT_EQ(strcmp(value, "hello"), 0);
  }

  // Test: Overwrite existing variable
  {
    ASSERT_EQ(putenv(replace_var), 0);
    char *value = getenv("PUTENV_TEST");
    ASSERT_TRUE(value != nullptr);
    ASSERT_EQ(strcmp(value, "world"), 0);
  }

  // Test: The pointer itself is used (not a copy)
  {
    // After putenv, getenv should return a pointer into the original string.
    char *value = getenv("PUTENV_TEST");
    ASSERT_TRUE(value == replace_var + 12); // "PUTENV_TEST=" is 12 chars
  }

  // Test: Empty value
  {
    ASSERT_EQ(putenv(empty_value), 0);
    char *value = getenv("PUTENV_EMPTY");
    ASSERT_TRUE(value != nullptr);
    ASSERT_EQ(strcmp(value, ""), 0);
  }

  // Test: Special characters in value
  {
    ASSERT_EQ(putenv(special_chars), 0);
    char *value = getenv("PUTENV_SPECIAL");
    ASSERT_TRUE(value != nullptr);
    ASSERT_EQ(strcmp(value, "!@#$%^&*()"), 0);
  }

  // Test: No '=' removes the variable (glibc/musl convention)
  {
    // First set a variable via putenv
    static char var_to_remove[] = "REMOVE_ME=present";
    ASSERT_EQ(putenv(var_to_remove), 0);
    ASSERT_TRUE(getenv("REMOVE_ME") != nullptr);

    // Now call putenv without '=' to remove it
    static char remove_cmd[] = "REMOVE_ME";
    ASSERT_EQ(putenv(remove_cmd), 0);
    ASSERT_TRUE(getenv("REMOVE_ME") == nullptr);
  }

  // Test: environ is updated and contains the exact pointer
  {
    bool found = false;
    for (char **env = environ; *env != nullptr; ++env) {
      if (*env == replace_var) {
        found = true;
        break;
      }
    }
    ASSERT_TRUE(found);
  }

  return 0;
}

} // namespace LIBC_NAMESPACE
