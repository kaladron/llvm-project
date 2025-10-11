//===-- Implementation of unsetenv ----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/stdlib/unsetenv.h"
#include "environ_internal.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/common.h"
#include "src/__support/libc_errno.h"
#include "src/__support/macros/config.h"

// We use extern "C" declarations for malloc/free instead of including
// src/stdlib/malloc.h and src/stdlib/free.h. This allows the implementation
// to work with different allocator implementations, particularly in integration
// tests which provide a simple bump allocator. The extern "C" linkage ensures
// we use whatever allocator is linked with the test or application.
extern "C" void *malloc(size_t);
extern "C" void free(void *);

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(int, unsetenv, (const char *name)) {
  // Validate input
  if (name == nullptr) {
    libc_errno = EINVAL;
    return -1;
  }

  cpp::string_view name_view(name);
  if (name_view.empty()) {
    libc_errno = EINVAL;
    return -1;
  }

  // POSIX: name cannot contain '='
  if (name_view.find_first_of('=') != cpp::string_view::npos) {
    libc_errno = EINVAL;
    return -1;
  }

  // Lock mutex for thread safety
  internal::environ_mutex.lock();

  // Initialize environ if not already done
  internal::init_environ();

  char **env_array = internal::get_environ_array();
  if (!env_array) {
    internal::environ_mutex.unlock();
    return 0; // Nothing to unset
  }

  // POSIX requires removing ALL occurrences, so we need to scan the whole array
  bool found_any = false;
  size_t write_pos = 0;

  for (size_t read_pos = 0; read_pos < internal::environ_size; read_pos++) {
    cpp::string_view current(env_array[read_pos]);
    bool matches = false;

    if (current.starts_with(name_view)) {
      // Check that name is followed by '='
      if (current.size() > name_view.size() &&
          current[name_view.size()] == '=') {
        matches = true;
        found_any = true;
      }
    }

    if (!matches) {
      // Keep this entry
      if (write_pos != read_pos) {
        env_array[write_pos] = env_array[read_pos];
        internal::environ_ownership[write_pos] =
            internal::environ_ownership[read_pos];
      }
      write_pos++;
    } else {
      // Removing this entry - free the string if we allocated it
      if (internal::environ_ownership[read_pos].can_free()) {
        free(env_array[read_pos]);
      }
    }
  }

  if (found_any) {
    internal::environ_size = write_pos;
    env_array[write_pos] = nullptr; // Maintain null terminator
  }

  internal::environ_mutex.unlock();
  return 0;
}

} // namespace LIBC_NAMESPACE_DECL
