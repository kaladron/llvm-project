//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of the POSIX putenv function.
///
//===----------------------------------------------------------------------===//

#include "src/stdlib/putenv.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/common.h"
#include "src/__support/libc_errno.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/null_check.h"
#include "src/stdlib/environ_internal.h"

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(int, putenv, (char *string)) {
  LIBC_CRASH_ON_NULLPTR(string);

  cpp::string_view sv(string);
  size_t eq_pos = sv.find_first_of('=');

  // Defense in depth: Reject empty variable names.
  // - eq_pos == 0: e.g. "=value" (empty name)
  // - eq_pos == cpp::string_view::npos && sv.empty(): e.g. "" (attempting to
  // unset empty name)
  if (eq_pos == 0 || (eq_pos == cpp::string_view::npos && sv.empty())) {
    libc_errno = EINVAL;
    return -1;
  }

  int result = internal::EnvironmentManager::get_instance().put(string);
  if (result != 0) {
    libc_errno = ENOMEM;
    return -1;
  }

  return 0;
}

} // namespace LIBC_NAMESPACE_DECL
