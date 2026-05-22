//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of regexec (Implementation).
///
//===----------------------------------------------------------------------===//

#include "src/regex/regexec.h"
#include "hdr/regex_macros.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/null_check.h"
#include "src/__support/regex/regex_internal.h"
#include "src/__support/regex/regex_matcher.h"

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(int, regexec,
                   (const regex_t *__restrict preg, const char *__restrict string,
                    size_t nmatch, regmatch_t *__restrict pmatch, int eflags)) {
  LIBC_CRASH_ON_NULLPTR(preg);
  LIBC_CRASH_ON_NULLPTR(string);

  if (!preg->__internal)
    return REG_BADPAT;

  RegexInternal *ri = reinterpret_cast<RegexInternal *>(preg->__internal);
  auto res = match(ri->pattern, cpp::string_view(string), nmatch, pmatch, eflags,
                   ri->pool);
  if (!res)
    return res.error();
  if (res.value() == MatchResult::Match)
    return 0;
  return REG_NOMATCH;
}

} // namespace LIBC_NAMESPACE_DECL
