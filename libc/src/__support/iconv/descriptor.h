//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Internal iconv encoding lookup and conversion descriptor definitions.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_ICONV_DESCRIPTOR_H
#define LLVM_LIBC_SRC___SUPPORT_ICONV_DESCRIPTOR_H

#include "hdr/errno_macros.h"
#include "hdr/stdint_proxy.h"
#include "hdr/types/iconv_t.h"
#include "hdr/types/size_t.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/ctype_utils.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace internal {

enum class EncodingId : uint8_t {
  Ascii,
  Iso8859_1,
};

struct EncodingAlias {
  cpp::string_view name;
  EncodingId id;
};

// Canonical aliases are lowercase with '-' and '_' stripped. Other punctuation
// such as '/' is preserved so '//' indicator suffixes fail with EINVAL.
LIBC_INLINE_VAR constexpr EncodingAlias ENCODING_ALIASES[] = {
    {"usascii", EncodingId::Ascii},      {"ascii", EncodingId::Ascii},
    {"ansix3.41968", EncodingId::Ascii}, {"iso646us", EncodingId::Ascii},
    {"646", EncodingId::Ascii},          {"iso88591", EncodingId::Iso8859_1},
    {"latin1", EncodingId::Iso8859_1},
};

LIBC_INLINE constexpr bool is_ignorable_sep(char c) {
  return c == '-' || c == '_';
}

LIBC_INLINE constexpr bool match_alias(cpp::string_view input,
                                       cpp::string_view canonical) {
  size_t j = 0;
  for (char c : input) {
    if (is_ignorable_sep(c))
      continue;
    if (j == canonical.size() || internal::tolower(c) != canonical[j])
      return false;
    ++j;
  }
  return j == canonical.size();
}

LIBC_INLINE constexpr cpp::optional<EncodingId>
find_encoding(cpp::string_view name) {
  for (const EncodingAlias &alias : ENCODING_ALIASES) {
    if (match_alias(name, alias.name))
      return alias.id;
  }
  return cpp::nullopt;
}

LIBC_INLINE iconv_t invalid_iconv_descriptor() {
  return reinterpret_cast<iconv_t>(-1);
}

LIBC_INLINE bool is_invalid_descriptor(iconv_t cd) {
  return cd == nullptr || cd == invalid_iconv_descriptor();
}

struct IconvDescriptor {
  EncodingId from;
  EncodingId to;

  LIBC_INLINE constexpr IconvDescriptor(EncodingId f, EncodingId t)
      : from(f), to(t) {}
};

LIBC_INLINE constexpr ErrorOr<IconvDescriptor>
resolve_descriptor(cpp::string_view tocode, cpp::string_view fromcode) {
  cpp::optional<EncodingId> to = find_encoding(tocode);
  if (!to.has_value())
    return Error(EINVAL);
  cpp::optional<EncodingId> from = find_encoding(fromcode);
  if (!from.has_value())
    return Error(EINVAL);
  return IconvDescriptor(*from, *to);
}

} // namespace internal
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_ICONV_DESCRIPTOR_H
