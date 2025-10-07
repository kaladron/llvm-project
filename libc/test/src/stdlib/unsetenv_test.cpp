//===-- Unittests for unsetenv --------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/stdlib/getenv.h"
#include "src/stdlib/setenv.h"
#include "src/stdlib/unsetenv.h"
#include "test/UnitTest/ErrnoCheckingTest.h"
#include "test/UnitTest/Test.h"

#include <errno.h>

TEST(LlvmLibcUnsetenvTest, Basic) {
  // Set a variable and then unset it
  ASSERT_EQ(LIBC_NAMESPACE::setenv("UNSET_TEST_VAR", "test_value", 1), 0);
  ASSERT_TRUE(LIBC_NAMESPACE::getenv("UNSET_TEST_VAR") != nullptr);
  
  // Unset the variable
  ASSERT_EQ(LIBC_NAMESPACE::unsetenv("UNSET_TEST_VAR"), 0);
  ASSERT_TRUE(LIBC_NAMESPACE::getenv("UNSET_TEST_VAR") == nullptr);
}

TEST(LlvmLibcUnsetenvTest, NonexistentVariable) {
  // Unsetting a nonexistent variable should succeed (not an error)
  ASSERT_EQ(LIBC_NAMESPACE::unsetenv("DOES_NOT_EXIST_VAR"), 0);
}

TEST(LlvmLibcUnsetenvTest, NullName) {
  errno = 0;
  ASSERT_EQ(LIBC_NAMESPACE::unsetenv(nullptr), -1);
  ASSERT_ERRNO_EQ(EINVAL);
}

TEST(LlvmLibcUnsetenvTest, EmptyName) {
  errno = 0;
  ASSERT_EQ(LIBC_NAMESPACE::unsetenv(""), -1);
  ASSERT_ERRNO_EQ(EINVAL);
}

TEST(LlvmLibcUnsetenvTest, NameWithEquals) {
  errno = 0;
  ASSERT_EQ(LIBC_NAMESPACE::unsetenv("BAD=NAME"), -1);
  ASSERT_ERRNO_EQ(EINVAL);
}

TEST(LlvmLibcUnsetenvTest, MultipleUnsets) {
  // Set multiple variables
  ASSERT_EQ(LIBC_NAMESPACE::setenv("UNSET_VAR1", "value1", 1), 0);
  ASSERT_EQ(LIBC_NAMESPACE::setenv("UNSET_VAR2", "value2", 1), 0);
  ASSERT_EQ(LIBC_NAMESPACE::setenv("UNSET_VAR3", "value3", 1), 0);
  
  // Verify all are set
  ASSERT_TRUE(LIBC_NAMESPACE::getenv("UNSET_VAR1") != nullptr);
  ASSERT_TRUE(LIBC_NAMESPACE::getenv("UNSET_VAR2") != nullptr);
  ASSERT_TRUE(LIBC_NAMESPACE::getenv("UNSET_VAR3") != nullptr);
  
  // Unset them one by one
  ASSERT_EQ(LIBC_NAMESPACE::unsetenv("UNSET_VAR1"), 0);
  ASSERT_TRUE(LIBC_NAMESPACE::getenv("UNSET_VAR1") == nullptr);
  ASSERT_TRUE(LIBC_NAMESPACE::getenv("UNSET_VAR2") != nullptr);
  ASSERT_TRUE(LIBC_NAMESPACE::getenv("UNSET_VAR3") != nullptr);
  
  ASSERT_EQ(LIBC_NAMESPACE::unsetenv("UNSET_VAR2"), 0);
  ASSERT_TRUE(LIBC_NAMESPACE::getenv("UNSET_VAR1") == nullptr);
  ASSERT_TRUE(LIBC_NAMESPACE::getenv("UNSET_VAR2") == nullptr);
  ASSERT_TRUE(LIBC_NAMESPACE::getenv("UNSET_VAR3") != nullptr);
  
  ASSERT_EQ(LIBC_NAMESPACE::unsetenv("UNSET_VAR3"), 0);
  ASSERT_TRUE(LIBC_NAMESPACE::getenv("UNSET_VAR1") == nullptr);
  ASSERT_TRUE(LIBC_NAMESPACE::getenv("UNSET_VAR2") == nullptr);
  ASSERT_TRUE(LIBC_NAMESPACE::getenv("UNSET_VAR3") == nullptr);
}

TEST(LlvmLibcUnsetenvTest, UnsetTwice) {
  // Set a variable
  ASSERT_EQ(LIBC_NAMESPACE::setenv("DOUBLE_UNSET_VAR", "value", 1), 0);
  ASSERT_TRUE(LIBC_NAMESPACE::getenv("DOUBLE_UNSET_VAR") != nullptr);
  
  // Unset it
  ASSERT_EQ(LIBC_NAMESPACE::unsetenv("DOUBLE_UNSET_VAR"), 0);
  ASSERT_TRUE(LIBC_NAMESPACE::getenv("DOUBLE_UNSET_VAR") == nullptr);
  
  // Unset it again (should succeed, not an error)
  ASSERT_EQ(LIBC_NAMESPACE::unsetenv("DOUBLE_UNSET_VAR"), 0);
  ASSERT_TRUE(LIBC_NAMESPACE::getenv("DOUBLE_UNSET_VAR") == nullptr);
}

TEST(LlvmLibcUnsetenvTest, SetUnsetSet) {
  // Set, unset, then set again
  ASSERT_EQ(LIBC_NAMESPACE::setenv("TOGGLE_VAR", "first", 1), 0);
  ASSERT_STREQ(LIBC_NAMESPACE::getenv("TOGGLE_VAR"), "first");
  
  ASSERT_EQ(LIBC_NAMESPACE::unsetenv("TOGGLE_VAR"), 0);
  ASSERT_TRUE(LIBC_NAMESPACE::getenv("TOGGLE_VAR") == nullptr);
  
  ASSERT_EQ(LIBC_NAMESPACE::setenv("TOGGLE_VAR", "second", 1), 0);
  ASSERT_STREQ(LIBC_NAMESPACE::getenv("TOGGLE_VAR"), "second");
  
  // Clean up
  LIBC_NAMESPACE::unsetenv("TOGGLE_VAR");
}

TEST(LlvmLibcUnsetenvTest, UnsetDoesNotAffectOthers) {
  // Set multiple variables
  ASSERT_EQ(LIBC_NAMESPACE::setenv("KEEP_VAR1", "keep1", 1), 0);
  ASSERT_EQ(LIBC_NAMESPACE::setenv("REMOVE_VAR", "remove", 1), 0);
  ASSERT_EQ(LIBC_NAMESPACE::setenv("KEEP_VAR2", "keep2", 1), 0);
  
  // Unset the middle one
  ASSERT_EQ(LIBC_NAMESPACE::unsetenv("REMOVE_VAR"), 0);
  
  // Verify the others are still present
  ASSERT_STREQ(LIBC_NAMESPACE::getenv("KEEP_VAR1"), "keep1");
  ASSERT_TRUE(LIBC_NAMESPACE::getenv("REMOVE_VAR") == nullptr);
  ASSERT_STREQ(LIBC_NAMESPACE::getenv("KEEP_VAR2"), "keep2");
  
  // Clean up
  LIBC_NAMESPACE::unsetenv("KEEP_VAR1");
  LIBC_NAMESPACE::unsetenv("KEEP_VAR2");
}

TEST(LlvmLibcUnsetenvTest, UnsetSimilarNames) {
  // Test that unsetting doesn't accidentally match partial names
  ASSERT_EQ(LIBC_NAMESPACE::setenv("VAR", "value1", 1), 0);
  ASSERT_EQ(LIBC_NAMESPACE::setenv("VARIABLE", "value2", 1), 0);
  ASSERT_EQ(LIBC_NAMESPACE::setenv("VAR_NAME", "value3", 1), 0);
  
  // Unset VAR should not affect VARIABLE or VAR_NAME
  ASSERT_EQ(LIBC_NAMESPACE::unsetenv("VAR"), 0);
  ASSERT_TRUE(LIBC_NAMESPACE::getenv("VAR") == nullptr);
  ASSERT_STREQ(LIBC_NAMESPACE::getenv("VARIABLE"), "value2");
  ASSERT_STREQ(LIBC_NAMESPACE::getenv("VAR_NAME"), "value3");
  
  // Clean up
  LIBC_NAMESPACE::unsetenv("VARIABLE");
  LIBC_NAMESPACE::unsetenv("VAR_NAME");
}
