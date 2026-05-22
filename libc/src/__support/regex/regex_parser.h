//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Parser for Regular Expressions (Prototypes).
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_REGEX_REGEX_PARSER_H
#define LLVM_LIBC_SRC___SUPPORT_REGEX_REGEX_PARSER_H

#include "src/__support/CPP/expected.h"
#include "src/__support/regex/regex_ast.h"
#include "src/__support/regex/regex_expr_pool.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

/// Parses an Extended Regular Expression (ERE) pattern into an AST.
///
/// \param pattern The null-terminated ERE pattern string.
/// \param pool The ExprPool to use for AST node allocation and hash-consing.
/// \returns An Expr pointer representing the root of the AST on success,
///          or a POSIX regex error code (REG_*) on failure.
cpp::expected<Expr *, int> parse_ere(const char *pattern, ExprPool &pool);

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_REGEX_REGEX_PARSER_H
