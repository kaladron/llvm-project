//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Bounded unsigned integer and scoped IPv4/IPv6 literal parser for netdb.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_NETDB_IP_PARSE_H
#define LLVM_LIBC_SRC___SUPPORT_NETDB_IP_PARSE_H

#include "hdr/stdint_proxy.h"
#include "hdr/types/size_t.h"
#include "hdr/types/struct_in6_addr.h"
#include "hdr/types/struct_in_addr.h"
#include "src/__support/CPP/array.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/OSUtil/network.h"
#include "src/__support/ctype_utils.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/net/address.h"
#include "src/__support/str_to_integer.h"
#include "src/string/memory_utils/inline_memcpy.h"

namespace LIBC_NAMESPACE_DECL {
namespace netdb {

struct ScopedIpAddr {
  bool is_ipv6 = false;
  cpp::array<uint8_t, 16> addr{};
  uint32_t scope_id = 0;
};

template <typename T>
LIBC_INLINE cpp::optional<T> parse_unsigned(cpp::string_view sv) {
  if (sv.empty() || !internal::isdigit(sv.front()))
    return cpp::nullopt;

  auto parsed = internal::strtointeger<T>(sv.data(), 10, sv.size());
  if (parsed.has_error() || static_cast<size_t>(parsed.parsed_len) != sv.size())
    return cpp::nullopt;

  return parsed.value;
}

LIBC_INLINE bool parse_ipv4(cpp::string_view tok, ScopedIpAddr &out) {
  struct in_addr v4{};
  if (!net::str_to_ipv4(tok, v4))
    return false;

  out = {};
  inline_memcpy(out.addr.data(), &v4, sizeof(v4));
  return true;
}

LIBC_INLINE bool parse_ipv6(cpp::string_view tok, ScopedIpAddr &out) {
  const size_t PCT = tok.find_first_of('%');
  struct in6_addr v6{};
  if (!net::str_to_ipv6(tok.substr(0, PCT), v6))
    return false;

  uint32_t scope = 0;
  if (PCT != cpp::string_view::npos) {
    const cpp::string_view ZONE = tok.substr(PCT + 1);
    const cpp::optional<uint32_t> PARSED = parse_unsigned<uint32_t>(ZONE);
    if (PARSED.has_value()) {
      scope = *PARSED;
    } else {
      scope = network::if_nametoindex(ZONE);
      if (scope == 0)
        return false;
    }
  }

  out = {};
  out.is_ipv6 = true;
  out.scope_id = scope;
  inline_memcpy(out.addr.data(), &v6, sizeof(v6));
  return true;
}

LIBC_INLINE bool parse_scoped_ip(cpp::string_view tok, ScopedIpAddr &out) {
  return parse_ipv4(tok, out) || parse_ipv6(tok, out);
}

} // namespace netdb
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_NETDB_IP_PARSE_H
