//===-- Standalone implementation of a char vector --------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_CHARVECTOR_H
#define LLVM_LIBC_SRC___SUPPORT_CHARVECTOR_H

#include "hdr/func/free.h"
#include "hdr/func/malloc.h"
#include "hdr/func/realloc.h"
#include "hdr/types/size_t.h"
#include "src/__support/CPP/span.h"
#include "src/__support/common.h" // LIBC_INLINE
#include "src/__support/macros/config.h"
#include "src/string/memory_utils/inline_memcpy.h"

namespace LIBC_NAMESPACE_DECL {

// This is very simple alternate of the std::string class. There is no
// bounds check performed in any of the methods. The callers are expected to
// do the checks before invoking the methods.
//
// This class will be extended as needed in future.

class CharVector {
  static constexpr size_t INIT_BUFF_SIZE = 64;
  char local_buffer[INIT_BUFF_SIZE];
  cpp::span<char> cur_str{local_buffer};
  size_t index = 0;

public:
  CharVector() = default;
  LIBC_INLINE ~CharVector() {
    if (cur_str.data() != local_buffer)
      free(cur_str.data());
  }

  // append returns true on success and false on allocation failure.
  LIBC_INLINE bool append(char new_char) {
    // Subtract 1 for index starting at 0 and another for the null terminator.
    if (index >= cur_str.size() - 2) {
      // If the new character would cause the string to be longer than the
      // buffer's size, attempt to allocate a new buffer.
      const size_t new_buff_size = cur_str.size() * 2;
      if (new_buff_size <= cur_str.size())
        return false;
      if (cur_str.data() == local_buffer) {
        char *new_str = reinterpret_cast<char *>(malloc(new_buff_size));
        if (new_str == nullptr)
          return false;
        inline_memcpy(new_str, cur_str.data(), index);
        cur_str = {new_str, new_buff_size};
      } else {
        char *new_str =
            reinterpret_cast<char *>(realloc(cur_str.data(), new_buff_size));
        if (new_str == nullptr)
          return false;
        cur_str = {new_str, new_buff_size};
      }
    }
    cur_str[index] = new_char;
    ++index;
    return true;
  }

  LIBC_INLINE char *c_str() {
    cur_str[index] = '\0';
    return cur_str.data();
  }

  LIBC_INLINE size_t length() { return index; }
};

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CHARVECTOR_H
