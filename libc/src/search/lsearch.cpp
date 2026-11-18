//===-- Implementation of lsearch -------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/search/lsearch.h"
#include "src/__support/CPP/cstddef.h" // cpp::byte
#include "src/__support/CPP/span.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/math_extras.h"
#include "src/string/memory_utils/inline_memcpy.h"

namespace LIBC_NAMESPACE_DECL {
LLVM_LIBC_FUNCTION(void *, lsearch,
                   (const void *key, void *base, size_t *nmemb, size_t size,
                    int (*compar)(const void *, const void *))) {
  if (key == nullptr || base == nullptr || nmemb == nullptr ||
      compar == nullptr)
    return nullptr;

  size_t byte_len = 0;
  if (mul_overflow(*nmemb, size, byte_len))
    return nullptr;

  cpp::span<const cpp::byte> remaining(
      reinterpret_cast<const cpp::byte *>(base), byte_len);
  while (!remaining.empty()) {
    if (compar(key, remaining.data()) == 0)
      return const_cast<cpp::byte *>(remaining.data());
    remaining = remaining.subspan(size);
  }

  size_t new_nmemb = 0;
  size_t total_bytes = 0;
  if (add_overflow(*nmemb, static_cast<size_t>(1), new_nmemb) ||
      add_overflow(byte_len, size, total_bytes))
    return nullptr;

  cpp::span<cpp::byte> appended =
      cpp::span<cpp::byte>(reinterpret_cast<cpp::byte *>(base), total_bytes)
          .last(size);
  *nmemb = new_nmemb;
  inline_memcpy(appended.data(), key, appended.size());
  return appended.data();
}

} // namespace LIBC_NAMESPACE_DECL
