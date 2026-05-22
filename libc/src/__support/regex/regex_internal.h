//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Internal structure for regex_t (Struct Definition).
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_REGEX_REGEX_INTERNAL_H
#define LLVM_LIBC_SRC___SUPPORT_REGEX_REGEX_INTERNAL_H

#include "src/__support/macros/config.h"
#include "src/__support/regex/regex_expr_pool.h"

namespace LIBC_NAMESPACE_DECL {

/// Internal representation of a compiled regular expression.
///
/// This structure is pointed to by the `__internal` member of `regex_t`.
/// It encapsulates the AST root and the ExprPool that owns the AST nodes.
struct RegexInternal {
  /// The pool that manages the AST nodes for this regex.
  ExprPool pool;
  /// The root of the compiled AST.
  Expr *pattern;
  /// The flags used during compilation.
  int cflags;

  /// Constructs a new internal regex state.
  RegexInternal(Expr *p, int flags) : pattern(p), cflags(flags) {}
};

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_REGEX_REGEX_INTERNAL_H
