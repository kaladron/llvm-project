//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Brzozowski derivative evaluation and regular expression matching interface.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_REGEX_REGEX_MATCHER_H
#define LLVM_LIBC_SRC___SUPPORT_REGEX_REGEX_MATCHER_H

#include "hdr/types/size_t.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/regex/regex_ast.h"
#include "src/__support/regex/regex_expr_pool.h"
#include "src/__support/regex/regex_internal.h"

namespace LIBC_NAMESPACE_DECL {
namespace regex {

/// Outcome of evaluating a regular expression against an input string.
enum class MatchResult {
  Match,
  NoMatch,
  OutOfMemory,
};

/// Byte offset range [start, end) of a whole-expression match within input.
struct MatchSpan {
  size_t start = 0;
  size_t end = 0;
};

/// Evaluates whether id can match the empty string at a fencepost with the
/// given active AnchorKind bitmask in O(1) via the precomputed 4-bit mask.
LIBC_INLINE bool nullable(ExprId id, uint8_t anchor_ctx, const ExprPool &pool) {
  return ((pool.get(id).nullable_mask >> (anchor_ctx & ALL_ANCHORS_MASK)) &
          1u) != 0;
}

/// Computes the Brzozowski derivative D_c(id) with respect to character c
/// when stepping across c from a fencepost with active anchor mask ctx_before.
///
/// Formally, D_c(L) = { w | c . w in L }.
ErrorOr<ExprId> derivative(ExprId id, char c, ExprPool &pool,
                           uint8_t ctx_before = 0);

/// Returns the reversed expression matching the string reversal of id,
/// swapping ANCHOR_BOL and ANCHOR_EOL.
ErrorOr<ExprId> reverse_expr(ExprId id, ExprPool &pool);

/// Evaluates whether pattern matches any substring of input using lazy
/// Brzozowski derivatives.
MatchResult match(ExprPool &pool, const ExprId pattern, cpp::string_view input,
                  int cflags = 0, int eflags = 0);

/// Evaluates a compiled RegexInternal against input.
LIBC_INLINE MatchResult match(RegexInternal &internal, cpp::string_view input,
                              int eflags = 0) {
  return match(internal.pool, internal.root, input, internal.cflags, eflags);
}

/// Finds the POSIX leftmost-longest whole-match byte span [start, end) of
/// pattern in input using a 3-pass bounded-horizon reverse scan.
/// reversed_pattern must be reverse_expr(pattern, pool).  Leaves span
/// untouched unless MatchResult::Match is returned.
MatchResult match_span(ExprPool &pool, const ExprId pattern,
                       const ExprId reversed_pattern, cpp::string_view input,
                       MatchSpan &span, int cflags = 0, int eflags = 0);

/// Finds the POSIX leftmost-longest whole-match byte span for a compiled
/// RegexInternal (must have been compiled without REG_NOSUB).
LIBC_INLINE MatchResult match_span(RegexInternal &internal,
                                   cpp::string_view input, MatchSpan &span,
                                   int eflags = 0) {
  return match_span(internal.pool, internal.root, internal.reversed_root, input,
                    span, internal.cflags, eflags);
}

} // namespace regex
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_REGEX_REGEX_MATCHER_H
