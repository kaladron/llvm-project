//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation header for regexec.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC_REGEX_REGEXEC_H
#define LLVM_LIBC_SRC_REGEX_REGEXEC_H

#include "hdr/types/regex_t.h"
#include "hdr/types/regmatch_t.h"
#include "hdr/types/size_t.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

/// Matches a precompiled regular expression against a string.
///
/// \param preg The precompiled regex_t structure.
/// \param string The null-terminated string to search.
/// \param nmatch Number of match results to store in pmatch.
/// \param pmatch Array to store match offsets.
/// \param eflags Execution flags (REG_NOTBOL, REG_NOTEOL).
/// \returns 0 on success (match found), REG_NOMATCH if no match found.
int regexec(const regex_t *__restrict preg, const char *__restrict string,
            size_t nmatch, regmatch_t *__restrict pmatch, int eflags);

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_REGEX_REGEXEC_H
