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

#ifndef LLVM_LIBC_SRC___SUPPORT_REGEX_REGEX_MATCHER_H
#define LLVM_LIBC_SRC___SUPPORT_REGEX_REGEX_MATCHER_H

#include "src/__support/regex/regex_ast.h"
#include "src/__support/regex/regex_expr_pool.h"
#include "include/llvm-libc-types/regmatch_t.h"

namespace LIBC_NAMESPACE_DECL {

/// Checks if an expression is nullable (i.e., it can match the empty string).
bool nullable(Expr *e);

/// Computes the Brzozowski derivative of an expression with respect to a
/// character.
///
/// The derivative of an expression e with respect to character c is a new
/// expression that matches all strings s such that c + s matches e.
Expr *derivative(Expr *e, char c, ExprPool &pool);

/// Matches a string against a compiled regex AST.
///
/// This function uses the derivative engine to determine if the pattern
/// matches any part of the string (unanchored by default).
///
/// \param pattern The root of the compiled regex AST.
/// \param string The string to match.
/// \param nmatch Number of match results requested (pmatch size).
/// \param pmatch Array to store match offsets (currently only pmatch[0] for
/// whole match).
/// \param eflags Standard POSIX regex execution flags (REG_NOTBOL, etc.).
/// \param pool The ExprPool that owns the AST nodes.
/// \returns 0 on success (match), REG_NOMATCH if no match found.
int match(Expr *pattern, const char *string, size_t nmatch, regmatch_t pmatch[],
          int eflags, ExprPool &pool);

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_REGEX_REGEX_MATCHER_H
