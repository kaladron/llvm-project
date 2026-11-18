//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Data structures for sorting routines.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC_STDLIB_QSORT_DATA_H
#define LLVM_LIBC_SRC_STDLIB_QSORT_DATA_H

#include "hdr/stdint_proxy.h"
#include "src/__support/CPP/cstddef.h"
#include "src/__support/CPP/span.h"
#include "src/__support/CPP/utility/swap.h"
#include "src/__support/macros/config.h"
#include "src/string/memory_utils/inline_memcpy.h"

namespace LIBC_NAMESPACE_DECL {
namespace internal {

class ArrayGenericSize {
  cpp::span<cpp::byte> array_bytes;
  size_t array_len;
  size_t elem_size;

  LIBC_INLINE ArrayGenericSize(cpp::span<cpp::byte> bytes, size_t s, size_t e)
      : array_bytes(bytes), array_len(s), elem_size(e) {}

  LIBC_INLINE cpp::span<cpp::byte> get_elem(size_t i) const {
    return array_bytes.subspan(i * elem_size, elem_size);
  }

public:
  LIBC_INLINE ArrayGenericSize(void *a, size_t s, size_t e)
      : array_bytes(reinterpret_cast<cpp::byte *>(a), s * e), array_len(s),
        elem_size(e) {}

  static constexpr bool has_fixed_size() { return false; }

  LIBC_INLINE void *get(size_t i) const { return get_elem(i).data(); }

  LIBC_INLINE void swap(size_t i, size_t j) const {
    // It's possible to use 8 byte blocks with `uint64_t`, but that
    // generates more machine code as the remainder loop gets
    // unrolled, plus 4 byte operations are more likely to be
    // efficient on a wider variety of hardware. On x86 LLVM tends
    // to unroll the block loop again into 2 16 byte swaps per
    // iteration which is another reason that 4 byte blocks yields
    // good performance even for big types.
    using block_t = uint32_t;
    constexpr size_t BLOCK_SIZE = sizeof(block_t);

    alignas(block_t) cpp::byte tmp_block[BLOCK_SIZE];

    cpp::span<cpp::byte> elem_i = get_elem(i);
    cpp::span<cpp::byte> elem_j = get_elem(j);

    while (elem_i.size() >= BLOCK_SIZE) {
      inline_memcpy(tmp_block, elem_i.data(), BLOCK_SIZE);
      inline_memcpy(elem_i.data(), elem_j.data(), BLOCK_SIZE);
      inline_memcpy(elem_j.data(), tmp_block, BLOCK_SIZE);

      elem_i = elem_i.subspan(BLOCK_SIZE);
      elem_j = elem_j.subspan(BLOCK_SIZE);
    }

    for (size_t n = 0; n < elem_i.size(); ++n)
      cpp::swap(elem_i[n], elem_j[n]);
  }

  LIBC_INLINE size_t len() const { return array_len; }

  // Make an Array starting at index |i| and length |s|.
  LIBC_INLINE ArrayGenericSize make_array(size_t i, size_t s) const {
    return ArrayGenericSize(array_bytes.subspan(i * elem_size, s * elem_size),
                            s, elem_size);
  }

  // Reset this Array to point at a different interval of the same
  // items starting at index |i|.
  LIBC_INLINE void reset_bounds(size_t i, size_t s) {
    array_bytes = array_bytes.subspan(i * elem_size, s * elem_size);
    array_len = s;
  }
};

// Having a specialized Array type for sorting that knows at
// compile-time what the size of the element is, allows for much more
// efficient swapping and for cheaper offset calculations.
template <size_t ELEM_SIZE> class ArrayFixedSize {
  cpp::span<cpp::byte> array_bytes;
  size_t array_len;

  LIBC_INLINE ArrayFixedSize(cpp::span<cpp::byte> bytes, size_t s)
      : array_bytes(bytes), array_len(s) {}

  LIBC_INLINE cpp::span<cpp::byte> get_elem(size_t i) const {
    return array_bytes.subspan(i * ELEM_SIZE, ELEM_SIZE);
  }

public:
  LIBC_INLINE ArrayFixedSize(void *a, size_t s)
      : array_bytes(reinterpret_cast<cpp::byte *>(a), s * ELEM_SIZE),
        array_len(s) {}

  // Beware this function is used a heuristic for cheap to swap types, so
  // instantiating `ArrayFixedSize` with `ELEM_SIZE > 100` is probably a bad
  // idea perf wise.
  static constexpr bool has_fixed_size() { return true; }

  LIBC_INLINE void *get(size_t i) const { return get_elem(i).data(); }

  LIBC_INLINE void swap(size_t i, size_t j) const {
    alignas(32) cpp::byte tmp[ELEM_SIZE];

    cpp::span<cpp::byte> elem_i = get_elem(i);
    cpp::span<cpp::byte> elem_j = get_elem(j);

    inline_memcpy(tmp, elem_i.data(), ELEM_SIZE);
    __builtin_memmove(elem_i.data(), elem_j.data(), ELEM_SIZE);
    inline_memcpy(elem_j.data(), tmp, ELEM_SIZE);
  }

  LIBC_INLINE size_t len() const { return array_len; }

  // Make an Array starting at index |i| and length |s|.
  LIBC_INLINE ArrayFixedSize<ELEM_SIZE> make_array(size_t i, size_t s) const {
    return ArrayFixedSize<ELEM_SIZE>(
        array_bytes.subspan(i * ELEM_SIZE, s * ELEM_SIZE), s);
  }

  // Reset this Array to point at a different interval of the same
  // items starting at index |i|.
  LIBC_INLINE void reset_bounds(size_t i, size_t s) {
    array_bytes = array_bytes.subspan(i * ELEM_SIZE, s * ELEM_SIZE);
    array_len = s;
  }
};

} // namespace internal
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_STDLIB_QSORT_DATA_H
