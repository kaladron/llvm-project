//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation header for regcomp (Prototypes).
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC_REGEX_REGCOMP_H
#define LLVM_LIBC_SRC_REGEX_REGCOMP_H

#include "hdr/types/regex_t.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

/// Compiles a regular expression into a form that can be used by regexec.
///
/// \param preg The regex_t structure to store the compiled regex.
/// \param pattern The null-terminated regular expression pattern.
/// \param cflags Compilation flags (REG_EXTENDED, REG_ICASE, etc.).
/// \returns 0 on success, or a REG_* error code on failure.
int regcomp(regex_t *__restrict preg, const char *__restrict pattern,
            int cflags);

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_REGEX_REGCOMP_H
