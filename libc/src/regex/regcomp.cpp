//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of regcomp.
///
//===----------------------------------------------------------------------===//

#include "src/regex/regcomp.h"
#include "include/llvm-libc-macros/regex-macros.h"
#include "src/__support/alloc-checker.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/null_check.h"
#include "src/__support/regex/regex_internal.h"
#include "src/__support/regex/regex_parser.h"

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(int, regcomp,
                   (regex_t *__restrict preg, const char *__restrict pattern,
                    int cflags)) {
  LIBC_CRASH_ON_NULLPTR(preg);
  LIBC_CRASH_ON_NULLPTR(pattern);

  preg->re_nsub = 0;
  AllocChecker ac;
  RegexInternal *ri = new (ac) RegexInternal(nullptr, cflags);
  if (!ac)
    return REG_ESPACE;

  auto res = parse_ere(pattern, ri->pool);
  if (!res) {
    int err = res.error();
    ri->~RegexInternal();
    ::operator delete(ri);
    return err;
  }

  ri->pattern = res.value();
  preg->__internal = ri;

  return 0;
}

} // namespace LIBC_NAMESPACE_DECL
