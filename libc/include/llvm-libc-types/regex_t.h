//===-- Definition of regex_t type ----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_REGEX_T_H
#define LLVM_LIBC_TYPES_REGEX_T_H

#include "size_t.h"

// POSIX requires at least re_nsub to be exposed. We add internal fields
// prefixed with __llvm_libc_ to store the compiled pattern.
typedef struct {
  size_t re_nsub;           // Number of parenthesized subexpressions.
  void *__llvm_libc_data;   // Internal: compiled pattern data.
  int __llvm_libc_cflags;   // Internal: compilation flags.
} regex_t;

#endif // LLVM_LIBC_TYPES_REGEX_T_H
