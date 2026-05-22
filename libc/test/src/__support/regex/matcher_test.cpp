//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Unit tests for regex parser and matcher logic.
///
//===----------------------------------------------------------------------===//

#include "src/__support/regex/regex_parser.h"
#include "src/__support/regex/regex_matcher.h"
#include "src/__support/regex/regex_expr_pool.h"
#include "test/UnitTest/Test.h"
#include "hdr/regex_macros.h"

using namespace LIBC_NAMESPACE::regex;

TEST(LlvmLibcRegexMatcherTest, BasicLiteralRoundTrip) {
  ExprPool pool;
  auto res = parse_ere("hello", pool);
  ASSERT_TRUE(res.has_value());
  auto pattern = res.value();

  auto match_res = match(pattern, "say hello world", 0, nullptr, 0, pool);
  ASSERT_TRUE(match_res.has_value());
  EXPECT_EQ(match_res.value(), MatchResult::Match);

  auto match_res2 = match(pattern, "goodbye", 0, nullptr, 0, pool);
  ASSERT_TRUE(match_res2.has_value());
  EXPECT_EQ(match_res2.value(), MatchResult::NoMatch);
}

TEST(LlvmLibcRegexMatcherTest, EmptyPattern) {
  ExprPool pool;
  auto res = parse_ere("", pool);
  ASSERT_TRUE(res.has_value());
  auto pattern = res.value();

  auto match_res = match(pattern, "anything", 0, nullptr, 0, pool);
  ASSERT_TRUE(match_res.has_value());
  EXPECT_EQ(match_res.value(), MatchResult::Match);

  auto match_res2 = match(pattern, "", 0, nullptr, 0, pool);
  ASSERT_TRUE(match_res2.has_value());
  EXPECT_EQ(match_res2.value(), MatchResult::Match);
}

TEST(LlvmLibcRegexMatcherTest, MultiCharLiteral) {
  ExprPool pool;
  auto res = parse_ere("world", pool);
  ASSERT_TRUE(res.has_value());
  auto pattern = res.value();

  auto match_res = match(pattern, "hello world", 0, nullptr, 0, pool);
  ASSERT_TRUE(match_res.has_value());
  EXPECT_EQ(match_res.value(), MatchResult::Match);

  auto match_res2 = match(pattern, "hello worl", 0, nullptr, 0, pool);
  ASSERT_TRUE(match_res2.has_value());
  EXPECT_EQ(match_res2.value(), MatchResult::NoMatch);
}

TEST(LlvmLibcRegexMatcherTest, LiteralPositions) {
  ExprPool pool;
  auto res = parse_ere("ab", pool);
  ASSERT_TRUE(res.has_value());
  auto pattern = res.value();

  EXPECT_EQ(match(pattern, "ab", 0, nullptr, 0, pool).value(), MatchResult::Match);
  EXPECT_EQ(match(pattern, "abc", 0, nullptr, 0, pool).value(), MatchResult::Match);
  EXPECT_EQ(match(pattern, "xab", 0, nullptr, 0, pool).value(), MatchResult::Match);
  EXPECT_EQ(match(pattern, "xabx", 0, nullptr, 0, pool).value(), MatchResult::Match);
  EXPECT_EQ(match(pattern, "axb", 0, nullptr, 0, pool).value(), MatchResult::NoMatch);
}

TEST(LlvmLibcRegexMatcherTest, SingleCharLiteral) {
  ExprPool pool;
  auto res = parse_ere("x", pool);
  ASSERT_TRUE(res.has_value());
  auto pattern = res.value();

  EXPECT_EQ(match(pattern, "x", 0, nullptr, 0, pool).value(), MatchResult::Match);
  EXPECT_EQ(match(pattern, "axb", 0, nullptr, 0, pool).value(), MatchResult::Match);
  EXPECT_EQ(match(pattern, "abc", 0, nullptr, 0, pool).value(), MatchResult::NoMatch);
}

TEST(LlvmLibcRegexMatcherTest, CaseSensitivity) {
  ExprPool pool;
  auto res = parse_ere("Hello", pool);
  ASSERT_TRUE(res.has_value());
  auto pattern = res.value();

  EXPECT_EQ(match(pattern, "hello", 0, nullptr, 0, pool).value(), MatchResult::NoMatch);
}
