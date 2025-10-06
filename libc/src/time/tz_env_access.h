//===-- Environment variable access for TZ -------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC_TIME_TZ_ENV_ACCESS_H
#define LLVM_LIBC_SRC_TIME_TZ_ENV_ACCESS_H

#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace time_internal {

// Get TZ environment variable value
// Returns nullptr if not set or not available
const char *get_tz_env();

} // namespace time_internal
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_TIME_TZ_ENV_ACCESS_H
