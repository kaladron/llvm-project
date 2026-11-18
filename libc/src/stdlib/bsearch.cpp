//===-- Implementation of bsearch -----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/stdlib/bsearch.h"
#include "src/__support/CPP/span.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/math_extras.h"

#include "hdr/stdint_proxy.h"

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(void *, bsearch,
                   (const void *key, const void *array, size_t array_size,
                    size_t elem_size,
                    int (*compare)(const void *, const void *))) {
  if (key == nullptr || array == nullptr || array_size == 0 || elem_size == 0)
    return nullptr;

  size_t byte_len = 0;
  if (mul_overflow(array_size, elem_size, byte_len))
    return nullptr;

  cpp::span<const uint8_t> bytes(reinterpret_cast<const uint8_t *>(array),
                                 byte_len);
  while (!bytes.empty()) {
    size_t mid = (bytes.size() / elem_size) / 2;
    size_t mid_offset = mid * elem_size;
    const void *elem = bytes.subspan(mid_offset, elem_size).data();
    int compare_result = compare(key, elem);
    if (compare_result < 0) {
      bytes = bytes.first(mid_offset);
    } else if (compare_result > 0) {
      bytes = bytes.subspan(mid_offset + elem_size);
    } else {
      return const_cast<void *>(elem);
    }
  }

  return nullptr;
}

} // namespace LIBC_NAMESPACE_DECL
