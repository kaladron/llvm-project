//===-- Implementation of internal environment utilities ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "environ_internal.h"
#include "config/app.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/macros/config.h"
#include "src/stdlib/free.h"
#include "src/stdlib/malloc.h"
#include "src/string/memcpy.h"

namespace LIBC_NAMESPACE_DECL {
namespace internal {

// Minimum initial capacity for the environment array when first allocated.
// This avoids frequent reallocations for small environments.
constexpr size_t MIN_ENVIRON_CAPACITY = 32;

// Growth factor for environment array capacity when expanding.
// When capacity is exceeded, new_capacity = old_capacity * ENVIRON_GROWTH_FACTOR.
constexpr size_t ENVIRON_GROWTH_FACTOR = 2;

// Global state for environment management
Mutex environ_mutex(false, false, false, false);
char **environ_storage = nullptr;
EnvStringOwnership *environ_ownership = nullptr;
size_t environ_capacity = 0;
size_t environ_size = 0;
bool environ_is_ours = false;

char **get_environ_array() {
  if (environ_is_ours)
    return environ_storage;
  return reinterpret_cast<char **>(LIBC_NAMESPACE::app.env_ptr);
}

void init_environ() {
  // Count entries in the startup environ
  char **env_ptr = reinterpret_cast<char **>(LIBC_NAMESPACE::app.env_ptr);
  if (!env_ptr)
    return;

  size_t count = 0;
  for (char **env = env_ptr; *env != nullptr; env++)
    count++;

  environ_size = count;
}

int find_env_var(cpp::string_view name) {
  char **env_array = get_environ_array();
  if (!env_array)
    return -1;

  for (size_t i = 0; i < environ_size; i++) {
    cpp::string_view current(env_array[i]);
    if (!current.starts_with(name))
      continue;

    // Check that name is followed by '='
    if (current.size() > name.size() && current[name.size()] == '=')
      return static_cast<int>(i);
  }

  return -1;
}

bool ensure_capacity(size_t needed) {
  // IMPORTANT: This function assumes environ_mutex is already held by the caller.
  // Do not add locking here as it would cause deadlock.
  
  // If we're still using the startup environ, we need to copy it
  if (!environ_is_ours) {
    char **old_env = reinterpret_cast<char **>(LIBC_NAMESPACE::app.env_ptr);

    // Allocate new array with room to grow
    size_t new_capacity = needed < MIN_ENVIRON_CAPACITY 
                          ? MIN_ENVIRON_CAPACITY 
                          : needed * ENVIRON_GROWTH_FACTOR;
    char **new_storage = reinterpret_cast<char **>(
        LIBC_NAMESPACE::malloc(sizeof(char *) * (new_capacity + 1)));
    if (!new_storage)
      return false;

    // Allocate ownership tracking array
    EnvStringOwnership *new_ownership = reinterpret_cast<EnvStringOwnership *>(
        LIBC_NAMESPACE::malloc(sizeof(EnvStringOwnership) * (new_capacity + 1)));
    if (!new_ownership) {
      LIBC_NAMESPACE::free(new_storage);
      return false;
    }

    // Copy existing pointers (we don't own the strings yet, so just copy pointers)
    if (old_env) {
      for (size_t i = 0; i < environ_size; i++) {
        new_storage[i] = old_env[i];
        // Initialize ownership: startup strings are not owned by us
        new_ownership[i] = EnvStringOwnership();
      }
    }
    new_storage[environ_size] = nullptr;

    environ_storage = new_storage;
    environ_ownership = new_ownership;
    environ_capacity = new_capacity;
    environ_is_ours = true;

    // Update app.env_ptr to point to our storage
    LIBC_NAMESPACE::app.env_ptr = reinterpret_cast<uintptr_t *>(environ_storage);

    return true;
  }

  // We already own environ, check if we need to grow it
  if (needed <= environ_capacity)
    return true;

  // Grow capacity by the growth factor
  size_t new_capacity = needed * ENVIRON_GROWTH_FACTOR;
  // Allocate new array and copy (realloc not available in libc yet)
  char **new_storage = reinterpret_cast<char **>(
      LIBC_NAMESPACE::malloc(sizeof(char *) * (new_capacity + 1)));
  if (!new_storage)
    return false;

  // Allocate new ownership array
  EnvStringOwnership *new_ownership = reinterpret_cast<EnvStringOwnership *>(
      LIBC_NAMESPACE::malloc(sizeof(EnvStringOwnership) * (new_capacity + 1)));
  if (!new_ownership) {
    LIBC_NAMESPACE::free(new_storage);
    return false;
  }

  // Copy old pointers and ownership info
  for (size_t i = 0; i < environ_size; i++) {
    new_storage[i] = environ_storage[i];
    new_ownership[i] = environ_ownership[i];
  }
  new_storage[environ_size] = nullptr;

  // Free old arrays (not the strings, just the arrays)
  LIBC_NAMESPACE::free(environ_storage);
  LIBC_NAMESPACE::free(environ_ownership);

  environ_storage = new_storage;
  environ_ownership = new_ownership;
  environ_capacity = new_capacity;

  // Update app.env_ptr to point to our new storage
  LIBC_NAMESPACE::app.env_ptr = reinterpret_cast<uintptr_t *>(environ_storage);

  return true;
}



} // namespace internal
} // namespace LIBC_NAMESPACE_DECL
