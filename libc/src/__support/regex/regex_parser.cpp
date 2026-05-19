//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Parser for Regular Expressions.
///
//===----------------------------------------------------------------------===//

#include "src/__support/regex/regex_parser.h"
#include "include/llvm-libc-macros/regex-macros.h"

namespace LIBC_NAMESPACE_DECL {

cpp::expected<Expr *, int> parse_ere(const char *pattern, ExprPool &pool) {
  Expr *res = pool.empty_str();
  if (!pattern)
    return res;

  while (*pattern) {
    Expr *lit = pool.make_lit(*pattern);
    if (!lit)
      return cpp::unexpected(REG_ESPACE);
    res = pool.make_concat(res, lit);
    if (!res)
      return cpp::unexpected(REG_ESPACE);
    pattern++;
  }
  return res;
}

} // namespace LIBC_NAMESPACE_DECL
