//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Basic tests for POSIX regex functions (ERE literals).
///
//===----------------------------------------------------------------------===//

#include "src/regex/regcomp.h"
#include "src/regex/regexec.h"
#include "src/regex/regfree.h"
#include "test/UnitTest/Test.h"

#include "hdr/regex_macros.h"

namespace LIBC_NAMESPACE {

TEST(LlvmLibcRegexTest, BasicLiteralRoundTrip) {
  regex_t preg;
  ASSERT_EQ(0, regcomp(&preg, "hello", REG_EXTENDED | REG_NOSUB));
  ASSERT_EQ(0, regexec(&preg, "say hello world", 0, nullptr, 0));
  ASSERT_EQ(REG_NOMATCH, regexec(&preg, "goodbye", 0, nullptr, 0));
  regfree(&preg);
}

TEST(LlvmLibcRegexTest, EmptyPattern) {
  regex_t preg;
  ASSERT_EQ(0, regcomp(&preg, "", REG_EXTENDED | REG_NOSUB));
  ASSERT_EQ(0, regexec(&preg, "anything", 0, nullptr, 0));
  ASSERT_EQ(0, regexec(&preg, "", 0, nullptr, 0));
  regfree(&preg);
}

TEST(LlvmLibcRegexTest, MultiCharLiteral) {
  regex_t preg;
  ASSERT_EQ(0, regcomp(&preg, "world", REG_EXTENDED | REG_NOSUB));
  ASSERT_EQ(0, regexec(&preg, "hello world", 0, nullptr, 0));
  ASSERT_EQ(REG_NOMATCH, regexec(&preg, "hello worl", 0, nullptr, 0));
  regfree(&preg);
}

TEST(LlvmLibcRegexTest, LiteralPositions) {
  regex_t preg;
  ASSERT_EQ(0, regcomp(&preg, "ab", REG_EXTENDED | REG_NOSUB));
  ASSERT_EQ(0, regexec(&preg, "ab", 0, nullptr, 0));       // exact
  ASSERT_EQ(0, regexec(&preg, "abc", 0, nullptr, 0));      // at start
  ASSERT_EQ(0, regexec(&preg, "xab", 0, nullptr, 0));      // at end
  ASSERT_EQ(0, regexec(&preg, "xabx", 0, nullptr, 0));     // in middle
  ASSERT_EQ(REG_NOMATCH, regexec(&preg, "axb", 0, nullptr, 0));
  regfree(&preg);
}

TEST(LlvmLibcRegexTest, SingleCharLiteral) {
  regex_t preg;
  ASSERT_EQ(0, regcomp(&preg, "x", REG_EXTENDED | REG_NOSUB));
  ASSERT_EQ(0, regexec(&preg, "x", 0, nullptr, 0));
  ASSERT_EQ(0, regexec(&preg, "axb", 0, nullptr, 0));
  ASSERT_EQ(REG_NOMATCH, regexec(&preg, "abc", 0, nullptr, 0));
  regfree(&preg);
}

TEST(LlvmLibcRegexTest, CaseSensitivity) {
  regex_t preg;
  ASSERT_EQ(0, regcomp(&preg, "Hello", REG_EXTENDED | REG_NOSUB));
  ASSERT_EQ(REG_NOMATCH, regexec(&preg, "hello", 0, nullptr, 0));
  regfree(&preg);
}
} // namespace LIBC_NAMESPACE
