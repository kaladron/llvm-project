//===-- Definition of regmatch_t type -------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_REGMATCH_T_H
#define LLVM_LIBC_TYPES_REGMATCH_T_H

#include "regoff_t.h"

// Structure to hold match offsets.
// rm_so is the byte offset from start of string to start of substring.
// rm_eo is the byte offset from start of string to first character after
// the end of substring.
typedef struct {
  regoff_t rm_so; // Byte offset from start of string to start of match.
  regoff_t rm_eo; // Byte offset from start of string to end of match.
} regmatch_t;

#endif // LLVM_LIBC_TYPES_REGMATCH_T_H
