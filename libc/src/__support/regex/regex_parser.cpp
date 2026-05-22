//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Parser for Regular Expressions (Implementation).
///
//===----------------------------------------------------------------------===//

#include "src/__support/regex/regex_parser.h"
#include "hdr/regex_macros.h"

namespace LIBC_NAMESPACE_DECL {

cpp::expected<Expr *, int> parse_ere(const char *pattern, ExprPool &pool) {
  auto res_expr = pool.empty_str();
  if (!res_expr)
    return res_expr;
  Expr *res = res_expr.value();

  if (!pattern)
    return res;

  while (*pattern) {
    auto lit_res = pool.make_lit(*pattern);
    if (!lit_res)
      return lit_res;
    auto concat_res = pool.make_concat(res, lit_res.value());
    if (!concat_res)
      return concat_res;
    res = concat_res.value();
    pattern++;
  }
  return res;
}

} // namespace LIBC_NAMESPACE_DECL
