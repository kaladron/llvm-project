//===-- Internal utilities for environment management ----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC_STDLIB_ENVIRON_INTERNAL_H
#define LLVM_LIBC_SRC_STDLIB_ENVIRON_INTERNAL_H

#include "hdr/types/size_t.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/CPP/utility.h"
#include "src/__support/macros/config.h"
#include "src/__support/threads/mutex.h"

namespace LIBC_NAMESPACE_DECL {
namespace internal {

// Global mutex protecting all environ modifications
extern Mutex environ_mutex;

// Our allocated environ array (nullptr if using startup environ)
extern char **environ_storage;

// Allocated capacity of environ_storage
extern size_t environ_capacity;

// Current number of variables in environ
extern size_t environ_size;

// True if we allocated environ_storage (and are responsible for freeing it)
extern bool environ_is_ours;

// Search for a variable by name in the current environ array.
// Returns the index if found, or -1 if not found.
// This function assumes the mutex is already held.
int find_env_var(cpp::string_view name);

// Ensure environ has capacity for at least `needed` entries (plus null terminator).
// May allocate or reallocate environ_storage.
// Returns true on success, false on allocation failure.
// This function assumes the mutex is already held.
bool ensure_capacity(size_t needed);

// Get a pointer to the current environ array.
// This may be app.env_ptr (startup environ) or environ_storage (our copy).
char **get_environ_array();

// Initialize environ management from the startup environment.
// This must be called before any setenv/unsetenv operations.
// This function is thread-safe and idempotent.
void init_environ();

} // namespace internal
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_STDLIB_ENVIRON_INTERNAL_H
