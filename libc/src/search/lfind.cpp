//===-- Implementation of lfind ---------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/search/lfind.h"
#include "src/__support/CPP/cstddef.h" // cpp::byte
#include "src/__support/CPP/span.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/math_extras.h"

namespace LIBC_NAMESPACE_DECL {
LLVM_LIBC_FUNCTION(void *, lfind,
                   (const void *key, const void *base, size_t *nmemb,
                    size_t size, int (*compar)(const void *, const void *))) {
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
  return nullptr;
}

} // namespace LIBC_NAMESPACE_DECL
