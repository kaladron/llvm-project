//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Submatch and pmatch contract tests for POSIX regexec.
///
//===----------------------------------------------------------------------===//

#include "hdr/regex_macros.h"
#include "hdr/types/regex_t.h"
#include "hdr/types/regmatch_t.h"
#include "hdr/types/regoff_t.h"
#include "hdr/types/size_t.h"
#include "src/__support/CPP/scope.h"
#include "src/regex/regcomp.h"
#include "src/regex/regexec.h"
#include "src/regex/regfree.h"
#include "test/UnitTest/Test.h"

using LIBC_NAMESPACE::cpp::scope_exit;

namespace {

void expect_span(const char *pattern, const char *input, regoff_t expected_so,
                 regoff_t expected_eo, int cflags = REG_EXTENDED,
                 int eflags = 0) {
  regex_t preg{};
  ASSERT_EQ(LIBC_NAMESPACE::regcomp(&preg, pattern, cflags), 0);
  scope_exit free_guard([&preg] { LIBC_NAMESPACE::regfree(&preg); });

  regmatch_t pm[1] = {{-9, -9}};
  ASSERT_EQ(LIBC_NAMESPACE::regexec(&preg, input, 1, pm, eflags), 0);
  EXPECT_EQ(pm[0].rm_so, expected_so);
  EXPECT_EQ(pm[0].rm_eo, expected_eo);
}

} // namespace

TEST(LlvmLibcRegexSubmatchTest, WholeMatchSpanAndPmatchContract) {
  regex_t preg{};
  ASSERT_EQ(LIBC_NAMESPACE::regcomp(&preg, "hello", REG_EXTENDED), 0);
  scope_exit free_guard([&preg] { LIBC_NAMESPACE::regfree(&preg); });

  regmatch_t pm[2] = {{42, 99}, {77, 88}};
  EXPECT_EQ(LIBC_NAMESPACE::regexec(&preg, "goodbye", 2, pm, 0), REG_NOMATCH);
  EXPECT_EQ(pm[0].rm_so, regoff_t(42));
  EXPECT_EQ(pm[0].rm_eo, regoff_t(99));
  EXPECT_EQ(pm[1].rm_so, regoff_t(77));
  EXPECT_EQ(pm[1].rm_eo, regoff_t(88));

  EXPECT_EQ(LIBC_NAMESPACE::regexec(&preg, "say hello", 2, pm, 0), 0);
  EXPECT_EQ(pm[0].rm_so, regoff_t(4));
  EXPECT_EQ(pm[0].rm_eo, regoff_t(9));
  EXPECT_EQ(pm[1].rm_so, regoff_t(-1));
  EXPECT_EQ(pm[1].rm_eo, regoff_t(-1));

  // nmatch == 0 allows pmatch == nullptr even without REG_NOSUB.
  EXPECT_EQ(LIBC_NAMESPACE::regexec(&preg, "say hello", 0, nullptr, 0), 0);

  // REG_NOSUB ignores nmatch and pmatch: nullptr succeeds and non-null pmatch
  // is left untouched on match.
  regex_t nosub{};
  ASSERT_EQ(LIBC_NAMESPACE::regcomp(&nosub, "hello", REG_EXTENDED | REG_NOSUB),
            0);
  scope_exit free_nosub([&nosub] { LIBC_NAMESPACE::regfree(&nosub); });
  EXPECT_EQ(LIBC_NAMESPACE::regexec(&nosub, "say hello", 2, nullptr, 0), 0);
  regmatch_t untouched[1] = {{11, 22}};
  EXPECT_EQ(LIBC_NAMESPACE::regexec(&nosub, "say hello", 1, untouched, 0), 0);
  EXPECT_EQ(untouched[0].rm_so, regoff_t(11));
  EXPECT_EQ(untouched[0].rm_eo, regoff_t(22));
}

TEST(LlvmLibcRegexSubmatchTest, LeftmostLongestCounterexamples) {
  expect_span("a", "aXa", 0, 1);
  expect_span("foo", "foo foo", 0, 3);
  expect_span("a|bcd", "abcd", 0, 1);
  expect_span("abcde|bcd", "abcde", 0, 5);
  expect_span("ab|bcde", "abcde", 0, 2);
  expect_span("a|b.*", "abz", 0, 1);
  expect_span("a*", "bbb", 0, 0);
  expect_span("a|ab", "ab", 0, 2);
  expect_span("b+", "abbbc", 1, 4);
  expect_span("foo", "hello foo world", 6, 9);

  // Near-miss non-match leaves caller-provided pmatch untouched.
  regex_t preg{};
  ASSERT_EQ(LIBC_NAMESPACE::regcomp(&preg, "b+d", REG_EXTENDED), 0);
  scope_exit free_guard([&preg] { LIBC_NAMESPACE::regfree(&preg); });
  regmatch_t pm[1] = {{5, 7}};
  EXPECT_EQ(LIBC_NAMESPACE::regexec(&preg, "abbbc", 1, pm, 0), REG_NOMATCH);
  EXPECT_EQ(pm[0].rm_so, regoff_t(5));
  EXPECT_EQ(pm[0].rm_eo, regoff_t(7));
}

TEST(LlvmLibcRegexSubmatchTest, NullableUnanchoredSpans) {
  // Leftmost start always wins; among matches at that start, longest end wins.
  expect_span("a*", "aa", 0, 2);
  expect_span("a?", "aa", 0, 1);
  expect_span("a*", "baa", 0, 0);
  expect_span("a+", "baa", 1, 3);
  expect_span("", "abc", 0, 0);
  expect_span("^|abc", "abc", 0, 3);
}

TEST(LlvmLibcRegexSubmatchTest, AnchorsNewlineAndEflags) {
  expect_span("^", "abc", 0, 0);
  expect_span("$", "a", 1, 1);
  expect_span("$", "abc", 3, 3);
  expect_span("^$", "", 0, 0);
  expect_span("^abc$", "abc", 0, 3);
  expect_span("^b$", "a\nb\nc", 2, 3, REG_EXTENDED | REG_NEWLINE, 0);
  expect_span("^a", "x\na", 2, 3, REG_EXTENDED | REG_NEWLINE, REG_NOTBOL);
  expect_span("a.*", "xab\nc", 1, 5, REG_EXTENDED, 0);
  expect_span("a.*", "xab\nc", 1, 3, REG_EXTENDED | REG_NEWLINE, 0);
  // Zero-width '^' matches at E1 = 2 while the earlier 'a' thread extends to 3.
  expect_span("a([a-z]|\n)*z|^", "a\nz", 0, 3, REG_EXTENDED | REG_NEWLINE, 0);

  regex_t bol_a{};
  ASSERT_EQ(LIBC_NAMESPACE::regcomp(&bol_a, "^a", REG_EXTENDED), 0);
  scope_exit free_bol([&bol_a] { LIBC_NAMESPACE::regfree(&bol_a); });
  regmatch_t pm[1] = {{3, 4}};
  EXPECT_EQ(LIBC_NAMESPACE::regexec(&bol_a, "a", 1, pm, REG_NOTBOL),
            REG_NOMATCH);
  EXPECT_EQ(pm[0].rm_so, regoff_t(3));
  EXPECT_EQ(pm[0].rm_eo, regoff_t(4));

  regex_t eol_b{};
  ASSERT_EQ(LIBC_NAMESPACE::regcomp(&eol_b, "b$", REG_EXTENDED), 0);
  scope_exit free_eol([&eol_b] { LIBC_NAMESPACE::regfree(&eol_b); });
  EXPECT_EQ(LIBC_NAMESPACE::regexec(&eol_b, "ab", 1, pm, REG_NOTEOL),
            REG_NOMATCH);
}

TEST(LlvmLibcRegexSubmatchTest, SubgroupGuardFailsClosed) {
  // Whole-match span (nmatch == 1) works even when the pattern has groups.
  regex_t grouped{};
  ASSERT_EQ(LIBC_NAMESPACE::regcomp(&grouped, "key=([a-z]+)", REG_EXTENDED), 0);
  scope_exit free_grouped([&grouped] { LIBC_NAMESPACE::regfree(&grouped); });
  ASSERT_EQ(grouped.re_nsub, size_t(1));

  regmatch_t pm1[1] = {{-9, -9}};
  EXPECT_EQ(LIBC_NAMESPACE::regexec(&grouped, "x key=abc y", 1, pm1, 0), 0);
  EXPECT_EQ(pm1[0].rm_so, regoff_t(2));
  EXPECT_EQ(pm1[0].rm_eo, regoff_t(9));

  // Requesting subgroup slots (nmatch > 1 && re_nsub > 0) fails closed with
  // REG_BADPAT on match (initialising pmatch to {-1, -1}) and still returns
  // REG_NOMATCH leaving pmatch untouched on non-match.
  regmatch_t pm2[2] = {{10, 20}, {30, 40}};
  EXPECT_EQ(LIBC_NAMESPACE::regexec(&grouped, "nomatch", 2, pm2, 0),
            REG_NOMATCH);
  EXPECT_EQ(pm2[0].rm_so, regoff_t(10));
  EXPECT_EQ(pm2[0].rm_eo, regoff_t(20));
  EXPECT_EQ(pm2[1].rm_so, regoff_t(30));
  EXPECT_EQ(pm2[1].rm_eo, regoff_t(40));

  EXPECT_EQ(LIBC_NAMESPACE::regexec(&grouped, "x key=abc y", 2, pm2, 0),
            REG_BADPAT);
  EXPECT_EQ(pm2[0].rm_so, regoff_t(-1));
  EXPECT_EQ(pm2[0].rm_eo, regoff_t(-1));
  EXPECT_EQ(pm2[1].rm_so, regoff_t(-1));
  EXPECT_EQ(pm2[1].rm_eo, regoff_t(-1));

  // Groupless pattern with nmatch > 1 fills pmatch[0] and sets trailing slots
  // to {-1, -1}.
  regex_t plain{};
  ASSERT_EQ(LIBC_NAMESPACE::regcomp(&plain, "key=[a-z]+", REG_EXTENDED), 0);
  scope_exit free_plain([&plain] { LIBC_NAMESPACE::regfree(&plain); });
  ASSERT_EQ(plain.re_nsub, size_t(0));
  regmatch_t pm3[3] = {{1, 2}, {3, 4}, {5, 6}};
  EXPECT_EQ(LIBC_NAMESPACE::regexec(&plain, "key=abc", 3, pm3, 0), 0);
  EXPECT_EQ(pm3[0].rm_so, regoff_t(0));
  EXPECT_EQ(pm3[0].rm_eo, regoff_t(7));
  EXPECT_EQ(pm3[1].rm_so, regoff_t(-1));
  EXPECT_EQ(pm3[1].rm_eo, regoff_t(-1));
  EXPECT_EQ(pm3[2].rm_so, regoff_t(-1));
  EXPECT_EQ(pm3[2].rm_eo, regoff_t(-1));
}

TEST(LlvmLibcRegexSubmatchTest, BoundedHorizonLongInput) {
  char buf[193];
  for (size_t i = 0; i < 192; ++i)
    buf[i] = 'x';
  buf[192] = '\0';

  // Match near the start of the buffer.
  buf[10] = 'f';
  buf[11] = 'o';
  buf[12] = 'o';
  expect_span("foo", buf, 10, 13);

  // Match near the end of the buffer.
  buf[10] = 'x';
  buf[11] = 'x';
  buf[12] = 'x';
  buf[180] = 'f';
  buf[181] = 'o';
  buf[182] = 'o';
  expect_span("foo", buf, 180, 183);

  // Earlier 'a' match stops Pass 1b immediately instead of chasing trailing b*.
  buf[0] = 'a';
  for (size_t i = 1; i < 192; ++i)
    buf[i] = 'b';
  expect_span("a|b.*", buf, 0, 1);
}

#if defined(LIBC_ADD_NULL_CHECKS)
TEST(LlvmLibcRegexSubmatchTest, PmatchNullPointerCrash) {
  regex_t preg{};
  ASSERT_EQ(LIBC_NAMESPACE::regcomp(&preg, "a", REG_EXTENDED), 0);
  scope_exit free_guard([&preg] { LIBC_NAMESPACE::regfree(&preg); });
  ASSERT_DEATH([&preg] { LIBC_NAMESPACE::regexec(&preg, "a", 1, nullptr, 0); },
               WITH_SIGNAL(-1));
  // Null check must trigger before matching even on a non-matching input.
  ASSERT_DEATH([&preg] { LIBC_NAMESPACE::regexec(&preg, "b", 1, nullptr, 0); },
               WITH_SIGNAL(-1));
}
#endif // defined(LIBC_ADD_NULL_CHECKS)
