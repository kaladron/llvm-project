//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of the inet_aton function.
///
//===----------------------------------------------------------------------===//

#include "src/arpa/inet/inet_aton.h"
#include "hdr/types/struct_in_addr.h"
#include "src/__support/common.h"
#include "src/__support/ctype_utils.h"
#include "src/__support/endian_internal.h"
#include "src/__support/str_to_integer.h"

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(int, inet_aton, (const char *cp, struct in_addr *inp)) {
  if (!cp || *cp == '\0')
    return 0;

  constexpr int IPV4_MAX_DOT_NUM = 3;
  unsigned long parts[IPV4_MAX_DOT_NUM + 1] = {0};
  uint32_t result = 0;
  int dot_num = 0;

  for (; dot_num <= IPV4_MAX_DOT_NUM; ++dot_num) {
    if (!internal::isdigit(*cp))
      return 0;

    auto result_part = internal::strtointeger<unsigned long>(cp, 0);
    if (result_part.has_error() || result_part.parsed_len == 0)
      return 0;

    parts[dot_num] = result_part;
    char next_char = *(cp + result_part.parsed_len);

    if (next_char != '.' && next_char != '\0')
      return 0;

    if (next_char == '\0')
      break;

    cp += (result_part.parsed_len + 1);
  }

  if (dot_num > IPV4_MAX_DOT_NUM)
    return 0;

  for (int i = 0; i <= dot_num; ++i) {
    unsigned long max_part =
        i == dot_num ? (0xffffffffUL >> (8 * dot_num)) : 0xffUL;
    if (parts[i] > max_part)
      return 0;
    int shift = i == dot_num ? 0 : 8 * (IPV4_MAX_DOT_NUM - i);
    result |= static_cast<uint32_t>(parts[i] << shift);
  }

  if (inp)
    inp->s_addr = Endian::to_big_endian(result);

  return 1;
}

} // namespace LIBC_NAMESPACE_DECL
