//===-- Portable string hash function ---------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_HASH_H
#define LLVM_LIBC_SRC___SUPPORT_HASH_H

#include "hdr/stdint_proxy.h"                // For uint64_t
#include "src/__support/CPP/bit.h"           // rotl
#include "src/__support/CPP/limits.h"        // numeric_limits
#include "src/__support/CPP/span.h"          // span
#include "src/__support/macros/attributes.h" // LIBC_INLINE
#include "src/__support/macros/config.h"
#include "src/__support/uint128.h" // UInt128
#include "src/string/memory_utils/inline_memcpy.h"

namespace LIBC_NAMESPACE_DECL {
namespace internal {

// Folded multiplication.
// This function multiplies two 64-bit integers and xor the high and
// low 64-bit parts of the result.
LIBC_INLINE uint64_t folded_multiply(uint64_t x, uint64_t y) {
  UInt128 p = static_cast<UInt128>(x) * static_cast<UInt128>(y);
  uint64_t low = static_cast<uint64_t>(p);
  uint64_t high = static_cast<uint64_t>(p >> 64);
  return low ^ high;
}

// Read as little endian.
// Shift-and-or implementation does not give a satisfactory code on aarch64.
// Therefore, we use a union to read the value.
template <typename T>
LIBC_INLINE T read_little_endian(cpp::span<const uint8_t> bytes) {
  cpp::span<const uint8_t> slice = bytes.first<sizeof(T)>();
  uint8_t buffer[sizeof(T)];
#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
  // Compiler should able to optimize this as a load followed by a byte
  // swap. On aarch64 (-mbig-endian), this compiles to the following for
  // int:
  //      ldr     w0, [x0]
  //      rev     w0, w0
  //      ret
  for (size_t i = 0; i < sizeof(T); ++i) {
    buffer[i] = slice[sizeof(T) - i - 1];
  }
#else
  inline_memcpy(buffer, slice.data(), sizeof(T));
#endif
  return cpp::bit_cast<T>(buffer);
}

template <typename T> LIBC_INLINE T read_little_endian(const void *ptr) {
  return read_little_endian<T>(
      cpp::span<const uint8_t>(static_cast<const uint8_t *>(ptr), sizeof(T)));
}

// Specialized read functions for small values. size must be <= 8.
LIBC_INLINE void read_small_values(cpp::span<const uint8_t> bytes,
                                   uint64_t &low, uint64_t &high) {
  if (bytes.size() >= 2) {
    if (bytes.size() >= 4) {
      low =
          static_cast<uint64_t>(read_little_endian<uint32_t>(bytes.first<4>()));
      high =
          static_cast<uint64_t>(read_little_endian<uint32_t>(bytes.last<4>()));
    } else {
      low =
          static_cast<uint64_t>(read_little_endian<uint16_t>(bytes.first<2>()));
      high = static_cast<uint64_t>(bytes.back());
    }
  } else {
    if (!bytes.empty()) {
      low = static_cast<uint64_t>(bytes.front());
      high = static_cast<uint64_t>(bytes.front());
    } else {
      low = 0;
      high = 0;
    }
  }
}

LIBC_INLINE void read_small_values(const void *ptr, size_t size, uint64_t &low,
                                   uint64_t &high) {
  read_small_values(
      cpp::span<const uint8_t>(static_cast<const uint8_t *>(ptr), size), low,
      high);
}

// This constant comes from Kunth's prng (it empirically works well).
LIBC_INLINE_VAR constexpr uint64_t MULTIPLE = 6364136223846793005;
// Rotation amount for mixing.
LIBC_INLINE_VAR constexpr uint64_t ROTATE = 23;

// Randomly generated values. For now, we use the same values as in aHash as
// they are widely tested.
// https://github.com/tkaitchuck/aHash/blob/9f6a2ad8b721fd28da8dc1d0b7996677b374357c/src/random_state.rs#L38
LIBC_INLINE_VAR constexpr uint64_t RANDOMNESS[2][4] = {
    {0x243f6a8885a308d3, 0x13198a2e03707344, 0xa4093822299f31d0,
     0x082efa98ec4e6c89},
    {0x452821e638d01377, 0xbe5466cf34e90c6c, 0xc0ac29b7c97c50dd,
     0x3f84d5b5b5470917},
};

// This is a portable string hasher. It is not cryptographically secure.
// The quality of the hash is good enough to pass all tests in SMHasher.
// The implementation is derived from the generic routine of aHash.
class HashState {
  uint64_t buffer;
  uint64_t pad;
  uint64_t extra_keys[2];
  LIBC_INLINE void update(uint64_t low, uint64_t high) {
    uint64_t combined =
        folded_multiply(low ^ extra_keys[0], high ^ extra_keys[1]);
    buffer = (buffer + pad) ^ combined;
    buffer = cpp::rotl(buffer, ROTATE);
  }
  LIBC_INLINE static uint64_t mix(uint64_t seed) {
    HashState mixer{RANDOMNESS[0][0], RANDOMNESS[0][1], RANDOMNESS[0][2],
                    RANDOMNESS[0][3]};
    mixer.update(seed, 0);
    return mixer.finish();
  }

public:
  LIBC_INLINE constexpr HashState(uint64_t a, uint64_t b, uint64_t c,
                                  uint64_t d)
      : buffer(a), pad(b), extra_keys{c, d} {}
  LIBC_INLINE HashState(uint64_t seed) {
    // Mix one more round of the seed to make it stronger.
    uint64_t mixed = mix(seed);
    buffer = RANDOMNESS[1][0] ^ mixed;
    pad = RANDOMNESS[1][1] ^ mixed;
    extra_keys[0] = RANDOMNESS[1][2] ^ mixed;
    extra_keys[1] = RANDOMNESS[1][3] ^ mixed;
  }
  LIBC_INLINE void update(cpp::span<const uint8_t> bytes) {
    buffer = (buffer + bytes.size()) * MULTIPLE;
    uint64_t low, high;
    if (bytes.size() > 8) {
      if (bytes.size() > 16) {
        // update tail
        cpp::span<const uint8_t> tail = bytes.last<16>();
        low = read_little_endian<uint64_t>(tail.first<8>());
        high = read_little_endian<uint64_t>(tail.last<8>());
        update(low, high);
        while (bytes.size() > 16) {
          low = read_little_endian<uint64_t>(bytes.first<8>());
          high = read_little_endian<uint64_t>(bytes.subspan<8, 8>());
          update(low, high);
          bytes = bytes.subspan(16);
        }
      } else {
        low = read_little_endian<uint64_t>(bytes.first<8>());
        high = read_little_endian<uint64_t>(bytes.last<8>());
        update(low, high);
      }
    } else {
      read_small_values(bytes, low, high);
      update(low, high);
    }
  }
  LIBC_INLINE void update(const void *ptr, size_t size) {
    update(cpp::span<const uint8_t>(static_cast<const uint8_t *>(ptr), size));
  }
  LIBC_INLINE uint64_t finish() {
    int rot = buffer & 63;
    uint64_t folded = folded_multiply(buffer, pad);
    return cpp::rotl(folded, rot);
  }
};

} // namespace internal
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_HASH_H
