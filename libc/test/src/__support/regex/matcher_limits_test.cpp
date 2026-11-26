//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Deep-chain, reversal, and span tests for the regex matcher.
///
/// These tests build expression chains thousands of nodes deep and exercise
/// reverse_expr() and match_span().  They live in their own binary because the
/// hermetic test heap is a fixed 1 MiB that is never freed, and matcher_test
/// already uses most of it.
///
//===----------------------------------------------------------------------===//

#include "hdr/regex_macros.h"
#include "hdr/types/size_t.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/error_or.h"
#include "src/__support/regex/regex_ast.h"
#include "src/__support/regex/regex_matcher.h"
#include "src/__support/regex/regex_parser.h"
#include "test/UnitTest/Test.h"
#include "test/src/__support/regex/expr_builder.h"

using LIBC_NAMESPACE::ErrorOr;
using LIBC_NAMESPACE::cpp::string_view;
using LIBC_NAMESPACE::regex::ANCHOR_BOL;
using LIBC_NAMESPACE::regex::ANCHOR_EOL;
using LIBC_NAMESPACE::regex::derivative;
using LIBC_NAMESPACE::regex::EMPTY_SET_ID;
using LIBC_NAMESPACE::regex::EMPTY_STR_ID;
using LIBC_NAMESPACE::regex::ExprId;
using LIBC_NAMESPACE::regex::INVALID_EXPR_ID;
using LIBC_NAMESPACE::regex::match;
using LIBC_NAMESPACE::regex::match_span;
using LIBC_NAMESPACE::regex::MatchResult;
using LIBC_NAMESPACE::regex::MatchSpan;
using LIBC_NAMESPACE::regex::parse_ere;
using LIBC_NAMESPACE::regex::REPEAT_INF;
using LIBC_NAMESPACE::regex::reverse_expr;
using LIBC_NAMESPACE::testing::ExprBuilder;

namespace {

// Builds the right-associated chain (a?)(a?)...(a?)b with `count` optional a's.
ExprId build_nullable_a_chain(ExprBuilder &b, size_t count) {
  const ExprId opt_a = b.alt(EMPTY_STR_ID, b.lit('a'));
  ExprId chain = b.lit('b');
  for (size_t i = 0; i < count; ++i)
    chain = b.cat(opt_a, chain);
  return chain;
}

ExprId deriv(ExprBuilder &b, ExprId id, char ch) {
  return ExprBuilder::unwrap(derivative(id, ch, b.raw()));
}

} // namespace

TEST(LlvmLibcRegexMatcherLimitsTest, DeepNullableConcatChain) {
  // A chain of 2,000 optional 'a's followed by 'b'.  Walking it needs about
  // 2,000 stack frames, far more than DerivStack keeps inline, so the stack
  // moves to the heap and grows several times.
  ExprBuilder b;
  const ExprId chain = build_nullable_a_chain(b, 2000);
  EXPECT_EQ(deriv(b, chain, 'b'), EMPTY_STR_ID);
  EXPECT_EQ(deriv(b, chain, 'x'), EMPTY_SET_ID);
  EXPECT_EQ(match(b.raw(), chain, "b"), MatchResult::Match);
  EXPECT_EQ(match(b.raw(), chain, "xxxx"), MatchResult::NoMatch);
}

TEST(LlvmLibcRegexMatcherLimitsTest, MultiByteChainReuseAndAltDerivative) {
  // A 24-element chain matched over several bytes.  The stack outgrows its
  // inline buffer on the first byte and match() keeps using it for the rest.
  // D_a produces a non-trivial 24-branch alternation across the grown stack.
  ExprBuilder b;
  const ExprId chain = build_nullable_a_chain(b, 24);
  EXPECT_EQ(b.verify_alt_spine(deriv(b, chain, 'a')), size_t(24));
  EXPECT_EQ(match(b.raw(), chain, "b"), MatchResult::Match);
  EXPECT_EQ(match(b.raw(), chain, "aaab"), MatchResult::Match);
  EXPECT_EQ(match(b.raw(), chain, "xxxx"), MatchResult::NoMatch);
}

TEST(LlvmLibcRegexMatcherLimitsTest, DeepAltSpine) {
  // 64 two-letter branches: 0..59 start with 'a', 60..63 with 'b'.  The walk
  // goes down the right side of every Alt with that Alt's left result saved.
  ExprBuilder b;
  ExprId spine = EMPTY_SET_ID;
  for (size_t i = 0; i < 64; ++i)
    spine = b.alt(spine, b.distinct_branch(i));
  EXPECT_EQ(b.verify_alt_spine(deriv(b, spine, 'a')), size_t(60));
  EXPECT_EQ(b.verify_alt_spine(deriv(b, spine, 'b')), size_t(4));
  EXPECT_EQ(deriv(b, spine, 'x'), EMPTY_SET_ID);
}

TEST(LlvmLibcRegexMatcherLimitsTest, AsymmetricNestedStackOrder) {
  // 10 nested (expr{1,2} . ch) wrappers around 'a' (depth 20 down the left
  // spine, growing DerivStack twice) with distinct suffixes 'b'..'k'.
  // Reversing or scrambling copied frames on stack growth produces the wrong
  // suffix order and fails the match checks.
  ExprBuilder b;
  ExprId expr = b.lit('a');
  for (size_t i = 0; i < 10; ++i)
    expr = b.cat(b.rep(expr, 1, 2), b.lit(static_cast<char>('b' + i)));
  EXPECT_EQ(match(b.raw(), expr, "abcdefghijk"), MatchResult::Match);
  EXPECT_EQ(match(b.raw(), expr, "akjihgfedcb"), MatchResult::NoMatch);
  EXPECT_EQ(match(b.raw(), expr, "abcdefghij"), MatchResult::NoMatch);
}

TEST(LlvmLibcRegexMatcherLimitsTest, RepeatDerivativeBoundaryRules) {
  ExprBuilder b;

  // Unbounded repeat preserves REPEAT_INF across differentiation:
  // D_a(a{3,inf}) == a{2,inf}.
  EXPECT_EQ(deriv(b, b.rep('a', 3, REPEAT_INF), 'a'),
            b.rep('a', 2, REPEAT_INF));

  // Conditionally nullable repeat body with a non-nullable suffix: at byte 0
  // ('^' matches), (^|a) is nullable so leading copies can match empty and
  // D_a((^|a){2,3}) drops min to 0, letting "ab" match.  Without '^'
  // (REG_NOTBOL or after a leading 'x'), (^|a) is not nullable so min only
  // drops to 1, rejecting "ab"/"xab" and requiring "aab".
  const ExprId rep_b =
      ExprBuilder::unwrap(parse_ere("(^|a){2,3}b", REG_EXTENDED, b.raw()));
  EXPECT_EQ(match(b.raw(), rep_b, "ab"), MatchResult::Match);
  EXPECT_EQ(match(b.raw(), rep_b, "ab", REG_EXTENDED, REG_NOTBOL),
            MatchResult::NoMatch);
  EXPECT_EQ(match(b.raw(), rep_b, "xab"), MatchResult::NoMatch);
  EXPECT_EQ(match(b.raw(), rep_b, "aab", REG_EXTENDED, REG_NOTBOL),
            MatchResult::Match);
}

TEST(LlvmLibcRegexMatcherLimitsTest, ReverseExprKindsAndInvolution) {
  ExprBuilder b;

  // Invalid inputs fail with REG_BADPAT; leaf nodes preserve identity except
  // directional anchors which swap BOL <-> EOL.
  ErrorOr<ExprId> bad0 = reverse_expr(INVALID_EXPR_ID, b.raw());
  ASSERT_FALSE(bad0.has_value());
  EXPECT_EQ(bad0.error(), REG_BADPAT);
  ErrorOr<ExprId> bad_oob = reverse_expr(9999, b.raw());
  ASSERT_FALSE(bad_oob.has_value());
  EXPECT_EQ(bad_oob.error(), REG_BADPAT);

  EXPECT_EQ(ExprBuilder::unwrap(reverse_expr(EMPTY_SET_ID, b.raw())),
            EMPTY_SET_ID);
  EXPECT_EQ(ExprBuilder::unwrap(reverse_expr(EMPTY_STR_ID, b.raw())),
            EMPTY_STR_ID);
  const ExprId lit_a = b.lit('a');
  EXPECT_EQ(ExprBuilder::unwrap(reverse_expr(lit_a, b.raw())), lit_a);
  const ExprId bs_az =
      ExprBuilder::unwrap(parse_ere("[a-z]", REG_EXTENDED, b.raw()));
  EXPECT_EQ(ExprBuilder::unwrap(reverse_expr(bs_az, b.raw())), bs_az);

  const ExprId bol = ExprBuilder::unwrap(b.raw().make_anchor(ANCHOR_BOL));
  const ExprId eol = ExprBuilder::unwrap(b.raw().make_anchor(ANCHOR_EOL));
  const ExprId both =
      ExprBuilder::unwrap(b.raw().make_anchor(ANCHOR_BOL | ANCHOR_EOL));
  EXPECT_EQ(ExprBuilder::unwrap(reverse_expr(bol, b.raw())), eol);
  EXPECT_EQ(ExprBuilder::unwrap(reverse_expr(eol, b.raw())), bol);
  EXPECT_EQ(ExprBuilder::unwrap(reverse_expr(both, b.raw())), both);

  // Compound kinds: Concat, Alt, Star, Repeat, and anchored Concat.
  const ExprId abc = b.cat(b.lit('a'), b.cat(b.lit('b'), b.lit('c')));
  const ExprId cba = b.cat(b.lit('c'), b.cat(b.lit('b'), b.lit('a')));
  EXPECT_EQ(ExprBuilder::unwrap(reverse_expr(abc, b.raw())), cba);
  EXPECT_EQ(ExprBuilder::unwrap(reverse_expr(cba, b.raw())), abc);

  const ExprId ab = b.cat(b.lit('a'), b.lit('b'));
  const ExprId ba = b.cat(b.lit('b'), b.lit('a'));
  const ExprId cd = b.cat(b.lit('c'), b.lit('d'));
  const ExprId dc = b.cat(b.lit('d'), b.lit('c'));
  EXPECT_EQ(ExprBuilder::unwrap(reverse_expr(b.alt(ab, cd), b.raw())),
            b.alt(ba, dc));
  EXPECT_EQ(ExprBuilder::unwrap(reverse_expr(b.star(ab), b.raw())), b.star(ba));
  EXPECT_EQ(ExprBuilder::unwrap(reverse_expr(b.rep(ab, 2, 4), b.raw())),
            b.rep(ba, 2, 4));

  const ExprId bol_ab = b.cat(bol, ab);
  const ExprId ba_eol = b.cat(ba, eol);
  EXPECT_EQ(ExprBuilder::unwrap(reverse_expr(bol_ab, b.raw())), ba_eol);
  EXPECT_EQ(ExprBuilder::unwrap(reverse_expr(ba_eol, b.raw())), bol_ab);

  const ExprId anchored = b.cat(bol, b.cat(ab, eol));
  const ExprId rev_anchored = b.cat(bol, b.cat(ba, eol));
  EXPECT_EQ(ExprBuilder::unwrap(reverse_expr(anchored, b.raw())), rev_anchored);
  EXPECT_EQ(ExprBuilder::unwrap(reverse_expr(rev_anchored, b.raw())), anchored);
}

TEST(LlvmLibcRegexMatcherLimitsTest, ReverseExprDeepChainsAndSharedDag) {
  // Reversing a 2,000-element Concat chain must allocate O(N) nodes (not
  // O(N^2) throwaway re-associations) and satisfy reverse(reverse(e)) == e.
  {
    ExprBuilder b;
    ExprId chain = b.lit('z');
    for (size_t i = 0; i < 1999; ++i)
      chain = b.cat(b.lit('a'), chain);
    const size_t before = b.raw().size();
    const ExprId rev = ExprBuilder::unwrap(reverse_expr(chain, b.raw()));
    EXPECT_LE(b.raw().size() - before, size_t(2005));
    EXPECT_EQ(ExprBuilder::unwrap(reverse_expr(rev, b.raw())), chain);
  }

  // Reversing a 64-branch Alt spine grows RevStack beyond its 4-frame inline
  // buffer and preserves double-reversal identity.
  {
    ExprBuilder b;
    ExprId spine = EMPTY_SET_ID;
    for (size_t i = 0; i < 64; ++i)
      spine = b.alt(spine, b.distinct_branch(i));
    const ExprId rev = ExprBuilder::unwrap(reverse_expr(spine, b.raw()));
    EXPECT_EQ(b.verify_alt_spine(rev), size_t(64));
    EXPECT_EQ(ExprBuilder::unwrap(reverse_expr(rev, b.raw())), spine);
  }

  // A 20-level diamond DAG has 2^20 tree paths across ~60 unique nodes; memo
  // keeps reversal linear and involution holds.
  {
    ExprBuilder b;
    ExprId dag = b.cat(b.lit('a'), b.lit('b'));
    for (size_t i = 0; i < 20; ++i) {
      const ExprId left = b.alt(dag, b.lit('c'));
      const ExprId right = b.alt(dag, b.lit('d'));
      dag = b.cat(left, right);
    }
    const size_t before = b.raw().size();
    const ExprId rev = ExprBuilder::unwrap(reverse_expr(dag, b.raw()));
    EXPECT_LE(b.raw().size() - before, before + 4);
    EXPECT_EQ(ExprBuilder::unwrap(reverse_expr(rev, b.raw())), dag);
  }
}

TEST(LlvmLibcRegexMatcherLimitsTest, MatchSpanDirectEdgeCases) {
  ExprBuilder b;
  MatchSpan span{99, 100};

  // Invalid ids and EMPTY_SET_ID return NoMatch without overwriting span.
  EXPECT_EQ(match_span(b.raw(), INVALID_EXPR_ID, EMPTY_STR_ID, "a", span),
            MatchResult::NoMatch);
  EXPECT_EQ(match_span(b.raw(), EMPTY_STR_ID, INVALID_EXPR_ID, "a", span),
            MatchResult::NoMatch);
  EXPECT_EQ(match_span(b.raw(), EMPTY_SET_ID, EMPTY_SET_ID, "a", span),
            MatchResult::NoMatch);
  EXPECT_EQ(span.start, size_t(99));
  EXPECT_EQ(span.end, size_t(100));

  // EMPTY_STR_ID matches [0, 0) immediately.
  EXPECT_EQ(match_span(b.raw(), EMPTY_STR_ID, EMPTY_STR_ID, "abc", span),
            MatchResult::Match);
  EXPECT_EQ(span.start, size_t(0));
  EXPECT_EQ(span.end, size_t(0));

  // Trailing universal .* extends across embedded NUL bytes in string_view,
  // whereas REG_NEWLINE excludes '\n' from '.' and stops before the newline.
  const ExprId dot_star =
      ExprBuilder::unwrap(parse_ere("a.*", REG_EXTENDED, b.raw()));
  const ExprId rev_dot_star =
      ExprBuilder::unwrap(reverse_expr(dot_star, b.raw()));
  static constexpr char WITH_NUL[] = {'x', 'a', '\0', 'b'};
  EXPECT_EQ(match_span(b.raw(), dot_star, rev_dot_star,
                       string_view(WITH_NUL, sizeof(WITH_NUL)), span,
                       REG_EXTENDED, 0),
            MatchResult::Match);
  EXPECT_EQ(span.start, size_t(1));
  EXPECT_EQ(span.end, size_t(4));

  const ExprId nl_dot_star = ExprBuilder::unwrap(
      parse_ere("a.*", REG_EXTENDED | REG_NEWLINE, b.raw()));
  const ExprId rev_nl_dot_star =
      ExprBuilder::unwrap(reverse_expr(nl_dot_star, b.raw()));
  EXPECT_EQ(match_span(b.raw(), nl_dot_star, rev_nl_dot_star, "xab\nc", span,
                       REG_EXTENDED | REG_NEWLINE, 0),
            MatchResult::Match);
  EXPECT_EQ(span.start, size_t(1));
  EXPECT_EQ(span.end, size_t(3));
}
