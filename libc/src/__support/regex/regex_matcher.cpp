//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Derivative-based regex matching engine.
///
//===----------------------------------------------------------------------===//

#include "src/__support/regex/regex_matcher.h"
#include "include/llvm-libc-macros/regex-macros.h"

namespace LIBC_NAMESPACE_DECL {

/// Nullability check (ε-closure).
///
/// Returns true if the expression can match the empty string.
/// - Ø (EmptySet) is not nullable.
/// - ε (EmptyStr) is nullable.
/// - Literals are not nullable.
/// - Concat is nullable if both sides are nullable.
/// - Alt is nullable if either side is nullable.
bool nullable(Expr *e) {
  if (!e)
    return false;
  switch (e->kind) {
  case ExprKind::EmptySet:
    return false;
  case ExprKind::EmptyStr:
    return true;
  case ExprKind::Literal:
    return false;
  case ExprKind::Concat:
    return nullable(e->bin.left) && nullable(e->bin.right);
  case ExprKind::Alt:
    return nullable(e->bin.left) || nullable(e->bin.right);
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
Expr *derivative(Expr *e, char c, ExprPool &pool) {
  if (!e)
    return nullptr;
  switch (e->kind) {
  case ExprKind::EmptySet:
  case ExprKind::EmptyStr:
    return pool.empty_set();
  case ExprKind::Literal:
    return (e->ch == c) ? pool.empty_str() : pool.empty_set();
  case ExprKind::Concat: {
    // Standard rule: ∂c(R·S) = (∂cR)·S | (ν(R)·∂cS)
    // where ν(R) is ε if R is nullable, else Ø.
    Expr *d_left = derivative(e->bin.left, c, pool);
    Expr *res = pool.make_concat(d_left, e->bin.right);
    if (!res)
      return nullptr;
    if (nullable(e->bin.left)) {
      Expr *d_right = derivative(e->bin.right, c, pool);
      res = pool.make_alt(res, d_right);
    }
    return res;
  }
  case ExprKind::Alt: {
    // Rule: ∂c(R|S) = ∂cR | ∂cS
    Expr *d_left = derivative(e->bin.left, c, pool);
    Expr *d_right = derivative(e->bin.right, c, pool);
    return pool.make_alt(d_left, d_right);
  }
  }
  return nullptr;
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
int match(Expr *pattern, const char *string, size_t, regmatch_t[], int,
          ExprPool &pool) {
  if (!pattern)
    return REG_BADPAT;

  // Empty pattern matches everything (it is nullable)
  if (nullable(pattern))
    return 0;

  Expr *state = pattern;
  if (!state)
    return REG_ESPACE;

  while (*string) {
    // Advance the match by computing the derivative and allowing a new
    // match to start at the current position (unanchored).
    state = pool.make_alt(pattern, derivative(state, *string, pool));
    if (!state)
      return REG_ESPACE;
    // If the current expression state can match the empty string, it means
    // some prefix of what we've seen so far matched the pattern.
    if (nullable(state))
      return 0;
    string++;
  }

  return REG_NOMATCH;
}

} // namespace LIBC_NAMESPACE_DECL
