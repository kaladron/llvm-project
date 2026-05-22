//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Derivative-based regex matching engine (Implementation).
///
//===----------------------------------------------------------------------===//

#include "src/__support/regex/regex_matcher.h"
#include "hdr/regex_macros.h"

namespace LIBC_NAMESPACE_DECL {
namespace regex {

/// Nullability check (ε-closure).
///
/// Returns true if the expression can match the empty string.
/// - Ø (EmptySet) is not nullable.
/// - ε (EmptyStr) is nullable.
/// - Literals are not nullable.
/// - Concat is nullable if both sides are nullable.
/// - Alt is nullable if either side is nullable.
bool nullable(ExprId id, const ExprPool &pool) {
  if (id == INVALID_EXPR_ID)
    return false;
  const Expr &e = pool.get(id);
  switch (e.kind) {
  case ExprKind::EmptySet:
    return false;
  case ExprKind::EmptyStr:
    return true;
  case ExprKind::Literal:
    return false;
  case ExprKind::Concat:
    return nullable(e.bin.left, pool) && nullable(e.bin.right, pool);
  case ExprKind::Alt:
    return nullable(e.bin.left, pool) || nullable(e.bin.right, pool);
  }
  return false;
}

/// Brzozowski derivative computation.
///
/// The derivative of R with respect to c (∂cR) represents the pattern that
/// must be matched by the remainder of the string if the first character was c.
///
/// Rules:
/// - ∂c(Ø) = Ø
/// - ∂c(ε) = Ø
/// - ∂c(a) = ε if a == c, else Ø
/// - ∂c(R | S) = ∂cR | ∂cS
/// - ∂c(R · S) = (∂cR · S) | ∂cS if R is nullable, else (∂cR · S)
cpp::expected<ExprId, int> derivative(ExprId id, char c, ExprPool &pool) {
  if (id == INVALID_EXPR_ID)
    return pool.empty_set();
  const Expr &e = pool.get(id);
  switch (e.kind) {
  case ExprKind::EmptySet:
  case ExprKind::EmptyStr:
    return pool.empty_set();
  case ExprKind::Literal:
    return (e.ch == c) ? pool.empty_str() : pool.empty_set();
  case ExprKind::Concat: {
    // Standard rule: ∂c(R·S) = (∂cR)·S | (ν(R)·∂cS)
    // where ν(R) is ε if R is nullable, else Ø.
    auto result_left = derivative(e.bin.left, c, pool);
    if (!result_left)
      return result_left;
    auto res = pool.make_concat(result_left.value(), e.bin.right);
    if (!res)
      return res;
    if (nullable(e.bin.left, pool)) {
      auto result_right = derivative(e.bin.right, c, pool);
      if (!result_right)
        return result_right;
      res = pool.make_alt(res.value(), result_right.value());
    }
    return res;
  }
  case ExprKind::Alt: {
    // Rule: ∂c(R|S) = ∂cR | ∂cS
    auto result_left = derivative(e.bin.left, c, pool);
    if (!result_left)
      return result_left;
    auto result_right = derivative(e.bin.right, c, pool);
    if (!result_right)
      return result_right;
    return pool.make_alt(result_left.value(), result_right.value());
  }
  }
  return cpp::unexpected(REG_BADPAT);
}

/// Matching loop.
///
/// For unanchored matching, we use the fact that matching pattern P against
/// any substring is equivalent to matching the pattern (Σ* · P) against the
/// string. In the derivative engine, this is implemented by alternating the
/// current state with the original pattern at every step:
///
///   state' = (∂c state) | pattern
///
/// This effectively "re-starts" the pattern at every character position.
cpp::expected<MatchResult, int> match(ExprId pattern, cpp::string_view string,
                                      size_t, regmatch_t[], int,
                                      ExprPool &pool) {
  if (pattern == INVALID_EXPR_ID)
    return cpp::unexpected(REG_BADPAT);

  // Empty pattern matches everything (it is nullable)
  if (nullable(pattern, pool))
    return MatchResult::Match;

  ExprId state = pattern;

  for (char c : string) {
    // Advance the match by computing the derivative and allowing a new
    // match to start at the current position (unanchored).
    auto d_res = derivative(state, c, pool);
    if (!d_res)
      return cpp::unexpected(d_res.error());
 
    auto next_state = pool.make_alt(pattern, d_res.value());
    if (!next_state)
      return cpp::unexpected(next_state.error());
 
    state = next_state.value();
 
    // If the current expression state can match the empty string, it means
    // some prefix of what we've seen so far matched the pattern.
    if (nullable(state, pool))
      return MatchResult::Match;
  }

  return MatchResult::NoMatch;
}

} // namespace regex
} // namespace LIBC_NAMESPACE_DECL
