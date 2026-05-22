//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of regfree (Implementation).
///
//===----------------------------------------------------------------------===//

#include "src/regex/regfree.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/null_check.h"
#include "src/__support/regex/regex_internal.h"
#include "src/__support/CPP/new.h"

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(void, regfree, (regex_t *preg)) {
  LIBC_CRASH_ON_NULLPTR(preg);

  if (!preg->__internal)
    return;

  RegexInternal *ri = reinterpret_cast<RegexInternal *>(preg->__internal);
  ri->~RegexInternal();
  ::operator delete(ri);
  preg->__internal = nullptr;
}

} // namespace LIBC_NAMESPACE_DECL
