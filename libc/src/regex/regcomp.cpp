//===-- Implementation of regcomp -------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/regex/regcomp.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(int, regcomp,
                   (regex_t *__restrict preg, const char *__restrict pattern,
                    int cflags)) {
  // TODO: Implement regex compilation.
  // For now, return REG_BADPAT to indicate not implemented.
  (void)preg;
  (void)pattern;
  (void)cflags;
  return REG_BADPAT;
}

} // namespace LIBC_NAMESPACE_DECL
