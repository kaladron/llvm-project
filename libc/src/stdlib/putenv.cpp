//===-- Implementation of putenv ------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/stdlib/putenv.h"
#include "environ_internal.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/common.h"
#include "src/__support/libc_errno.h"
#include "src/__support/macros/config.h"

// We use extern "C" declaration for free instead of including
// src/stdlib/free.h. This allows the implementation to work with different
// allocator implementations, particularly in integration tests which provide a
// simple bump allocator. The extern "C" linkage ensures we use whatever
// allocator is linked with the test or application.
extern "C" void free(void *);

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(int, putenv, (char *string)) {
  // Validate input
  if (string == nullptr) {
    libc_errno = EINVAL;
    return -1;
  }

  cpp::string_view string_view(string);

  // POSIX: string must contain '='
  size_t equals_pos = string_view.find_first_of('=');
  if (equals_pos == cpp::string_view::npos) {
    libc_errno = EINVAL;
    return -1;
  }

  // Extract the name part (before '=')
  cpp::string_view name = string_view.substr(0, equals_pos);

  // Name cannot be empty
  if (name.empty()) {
    libc_errno = EINVAL;
    return -1;
  }

  // Lock mutex for thread safety
  internal::environ_mutex.lock();

  // Initialize environ if not already done
  internal::init_environ();

  // Search for existing variable with the same name
  int index = internal::find_env_var(name);

  if (index >= 0) {
    // Variable exists - replace it
    char **env_array = internal::get_environ_array();

    // Free old string only if we allocated it
    // Don't free if it was from startup environ or a previous putenv
    if (internal::environ_ownership[index].can_free()) {
      free(env_array[index]);
    }

    // Replace with the provided string pointer
    // CRITICAL: We do NOT copy the string - we use the caller's pointer
    env_array[index] = string;

    // Mark this string as NOT allocated by us (caller owns the memory)
    internal::environ_ownership[index].allocated_by_us = false;

    internal::environ_mutex.unlock();
    return 0;
  }

  // Variable doesn't exist - add it
  // Ensure we have capacity for one more entry
  if (!internal::ensure_capacity(internal::environ_size + 1)) {
    internal::environ_mutex.unlock();
    libc_errno = ENOMEM;
    return -1;
  }

  // Add the string pointer to environ array
  // CRITICAL: We do NOT copy the string - we use the caller's pointer
  char **env_array = internal::get_environ_array();
  env_array[internal::environ_size] = string;

  // Mark this string as NOT allocated by us (caller owns the memory)
  internal::environ_ownership[internal::environ_size].allocated_by_us = false;

  internal::environ_size++;

  // Ensure null terminator
  env_array[internal::environ_size] = nullptr;

  internal::environ_mutex.unlock();
  return 0;
}

} // namespace LIBC_NAMESPACE_DECL
