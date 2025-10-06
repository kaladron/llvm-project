//===-- Environment variable access for TZ -------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "tz_env_access.h"

// Forward declaration for system getenv
// In overlay/unit test mode, this will link to system libc
// In hermetic mode, this may not be available (returns nullptr)
extern "C" char *getenv(const char *) __attribute__((weak));

namespace LIBC_NAMESPACE_DECL {
namespace time_internal {

const char *get_tz_env() {
  // Use system getenv if available
  // This works in overlay and unit test modes where system libc is available
  // In hermetic mode, getenv may not be linked (weak symbol will be null)
  if (::getenv != nullptr) {
    return ::getenv("TZ");
  }

  // No environment access available (e.g., hermetic mode without getenv)
  return nullptr;
}

} // namespace time_internal
} // namespace LIBC_NAMESPACE_DECL
