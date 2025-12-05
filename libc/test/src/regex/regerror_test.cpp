//===-- Unittests for regerror --------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/regex/regerror.h"
#include "test/UnitTest/Test.h"

TEST(LlvmLibcRegErrorTest, Success) {
  char buf[256];
  size_t needed = LIBC_NAMESPACE::regerror(0, nullptr, buf, sizeof(buf));
  ASSERT_GT(needed, size_t(0));
  ASSERT_STREQ(buf, "Success");
  // Return value should be length + 1 for null terminator
  ASSERT_EQ(needed, size_t(8)); // "Success" + null
}

TEST(LlvmLibcRegErrorTest, AllErrorCodes) {
  char buf[256];

  // Test all defined error codes
  struct TestCase {
    int code;
    const char *expected_substring;
  };

  TestCase cases[] = {
      {REG_NOMATCH, "No match"},
      {REG_BADPAT, "Invalid regular expression"},
      {REG_ECOLLATE, "collating"},
      {REG_ECTYPE, "character class"},
      {REG_EESCAPE, "backslash"},
      {REG_ESUBREG, "back reference"},
      {REG_EBRACK, "["},
      {REG_EPAREN, "("},
      {REG_EBRACE, "{"},
      {REG_BADBR, "{}"},
      {REG_ERANGE, "range"},
      {REG_ESPACE, "Memory"},
      {REG_BADRPT, "preceding"},
  };

  for (const auto &tc : cases) {
    size_t needed = LIBC_NAMESPACE::regerror(tc.code, nullptr, buf, sizeof(buf));
    ASSERT_GT(needed, size_t(0));
    // Ensure null-terminated and within bounds
    ASSERT_LT(needed, sizeof(buf));
    // Verify the message is not empty (has content before null terminator)
    ASSERT_GT(buf[0], '\0');
  }
}

TEST(LlvmLibcRegErrorTest, UnknownError) {
  char buf[256];
  // Use an error code that doesn't exist
  size_t needed = LIBC_NAMESPACE::regerror(9999, nullptr, buf, sizeof(buf));
  ASSERT_GT(needed, size_t(0));
  ASSERT_STREQ(buf, "Unknown error");
}

TEST(LlvmLibcRegErrorTest, NullBuffer) {
  // When buffer is null, should still return required size
  size_t needed = LIBC_NAMESPACE::regerror(REG_NOMATCH, nullptr, nullptr, 0);
  ASSERT_GT(needed, size_t(0));
  ASSERT_EQ(needed, size_t(9)); // "No match" + null
}

TEST(LlvmLibcRegErrorTest, ZeroBufferSize) {
  char buf[256] = "untouched";
  // When buffer size is 0, buffer should not be modified
  size_t needed = LIBC_NAMESPACE::regerror(REG_NOMATCH, nullptr, buf, 0);
  ASSERT_GT(needed, size_t(0));
  ASSERT_STREQ(buf, "untouched");
}

TEST(LlvmLibcRegErrorTest, SmallBuffer) {
  char buf[5];
  // Buffer too small - should truncate and null-terminate
  size_t needed = LIBC_NAMESPACE::regerror(REG_NOMATCH, nullptr, buf, sizeof(buf));
  ASSERT_EQ(needed, size_t(9)); // Still returns full size needed
  ASSERT_EQ(buf[4], '\0');      // Must be null-terminated
  ASSERT_STREQ(buf, "No m");    // Truncated to 4 chars + null
}

TEST(LlvmLibcRegErrorTest, ExactSizeBuffer) {
  char buf[9]; // Exactly "No match" + null
  size_t needed = LIBC_NAMESPACE::regerror(REG_NOMATCH, nullptr, buf, sizeof(buf));
  ASSERT_EQ(needed, size_t(9));
  ASSERT_STREQ(buf, "No match");
}

TEST(LlvmLibcRegErrorTest, OneByteBuffer) {
  char buf[1];
  size_t needed = LIBC_NAMESPACE::regerror(REG_NOMATCH, nullptr, buf, sizeof(buf));
  ASSERT_EQ(needed, size_t(9));
  ASSERT_EQ(buf[0], '\0'); // Only null terminator fits
}
