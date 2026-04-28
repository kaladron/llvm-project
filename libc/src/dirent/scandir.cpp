//===-- Implementation of scandir -----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "scandir.h"

#include "hdr/func/free.h"
#include "hdr/func/malloc.h"
#include "hdr/func/realloc.h"
#include "hdr/types/struct_dirent.h"
#include "src/__support/File/scoped_dir.h"
#include "src/__support/common.h"
#include "src/__support/libc_errno.h"
#include "src/__support/macros/config.h"
#include "src/stdlib/qsort_util.h"
#include "src/string/memory_utils/inline_memcpy.h"

namespace LIBC_NAMESPACE_DECL {

// scandir returns malloc-allocated memory to the caller, who is responsible
// for calling free() on each entry and on the returned array. Using malloc
// and realloc directly here is intentional to match this API contract.

LLVM_LIBC_FUNCTION(int, scandir,
                   (const char *dirp, struct dirent ***namelist,
                    __scandir_filter_t filter,
                    __scandir_compare_t compar)) {
  // Open the directory.
  auto dir_or_err = Dir::open(dirp);
  if (!dir_or_err) {
    libc_errno = dir_or_err.error();
    return -1;
  }
  ScopedDir dir(dir_or_err.value());

  struct dirent **list = nullptr;
  size_t size = 0;
  size_t capacity = 0;

  // Free all entries collected so far on error.
  auto cleanup = [&]() {
    for (size_t i = 0; i < size; ++i)
      ::free(list[i]);
    ::free(list);
  };

  // Read and filter directory entries.
  for (;;) {
    auto dirent_val = dir->read();
    if (!dirent_val) {
      libc_errno = dirent_val.error();
      cleanup();
      return -1;
    }
    struct dirent *d = dirent_val.value();
    if (d == nullptr)
      break;

    if (filter && !filter(d))
      continue;

    // Grow the result array when full.
    if (size >= capacity) {
      size_t new_capacity = capacity == 0 ? 4 : capacity * 2;
      struct dirent **new_list = reinterpret_cast<struct dirent **>(
          ::realloc(list, new_capacity * sizeof(struct dirent *)));
      if (!new_list) {
        libc_errno = ENOMEM;
        cleanup();
        return -1;
      }
      list = new_list;
      capacity = new_capacity;
    }

    // Copy the entry for the caller to own.
    size_t d_size = d->d_reclen;
    struct dirent *copy =
        reinterpret_cast<struct dirent *>(::malloc(d_size));
    if (!copy) {
      libc_errno = ENOMEM;
      cleanup();
      return -1;
    }
    inline_memcpy(copy, d, d_size);
    list[size++] = copy;
  }

  // Sort the collected entries if a comparator was provided.
  if (compar && size > 0) {
    auto is_less = [compar](const void *a, const void *b) -> bool {
      return compar(
                 reinterpret_cast<const struct dirent **>(const_cast<void *>(a)),
                 reinterpret_cast<const struct dirent **>(const_cast<void *>(b))) <
             0;
    };
    internal::unstable_sort(list, size, sizeof(struct dirent *), is_less);
  }

  *namelist = list;
  return static_cast<int>(size);
}

} // namespace LIBC_NAMESPACE_DECL
