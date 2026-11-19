//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Reverse DNS PTR QNAME builders, RFC 952/1123 check, and NI_NOFQDN trimmer.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_NETDB_DNS_PTR_H
#define LLVM_LIBC_SRC___SUPPORT_NETDB_DNS_PTR_H

#include "hdr/stdint_proxy.h"
#include "hdr/types/size_t.h"
#include "src/__support/CPP/array.h"
#include "src/__support/CPP/span.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/integer_to_string.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/netdb/dns_wire.h"
#include "src/__support/netdb/hosts.h"
#include "src/string/memory_utils/inline_memcpy.h"

namespace LIBC_NAMESPACE_DECL {
namespace netdb {

constexpr size_t IPV6_PTR_WIRE_LEN = 74;

LIBC_INLINE size_t
build_ipv4_ptr_qname(cpp::span<const uint8_t> ipv4,
                     cpp::array<uint8_t, IPV6_PTR_WIRE_LEN> &out) {
  static constexpr uint8_t SUFFIX[14] = {
      7, 'i', 'n', '-', 'a', 'd', 'd', 'r', 4, 'a', 'r', 'p', 'a', 0,
  };
  size_t pos = 0;
  for (size_t i = 0; i < 4; ++i) {
    const IntegerToString<uint8_t> OCTET(ipv4[3 - i]);
    const cpp::string_view VIEW = OCTET.view();
    out[pos++] = static_cast<uint8_t>(VIEW.size());
    for (char ch : VIEW)
      out[pos++] = static_cast<uint8_t>(ch);
  }
  inline_memcpy(out.data() + pos, SUFFIX, sizeof(SUFFIX));
  return pos + sizeof(SUFFIX);
}

LIBC_INLINE size_t
build_ipv6_ptr_qname(cpp::span<const uint8_t> ipv6,
                     cpp::array<uint8_t, IPV6_PTR_WIRE_LEN> &out) {
  static constexpr char HEX[] = "0123456789abcdef";
  static constexpr uint8_t SUFFIX[10] = {
      3, 'i', 'p', '6', 4, 'a', 'r', 'p', 'a', 0,
  };
  size_t pos = 0;
  for (size_t i = 0; i < 16; ++i) {
    const uint8_t BYTE = ipv6[15 - i];
    out[pos++] = 1;
    out[pos++] = static_cast<uint8_t>(HEX[BYTE & 0x0fu]);
    out[pos++] = 1;
    out[pos++] = static_cast<uint8_t>(HEX[BYTE >> 4]);
  }
  inline_memcpy(out.data() + pos, SUFFIX, sizeof(SUFFIX));
  return pos + sizeof(SUFFIX);
}

LIBC_INLINE bool is_valid_ptr_hostname(cpp::string_view name) {
  size_t pos = 0;
  while (true) {
    const size_t DOT = name.find_first_of('.', pos);
    const size_t END = DOT == cpp::string_view::npos ? name.size() : DOT;
    if (!is_valid_host_label(name.substr(pos, END - pos)))
      return false;
    if (DOT == cpp::string_view::npos)
      return true;
    pos = DOT + 1;
  }
}

LIBC_INLINE cpp::string_view apply_ni_nofqdn(cpp::string_view fqdn,
                                             cpp::string_view local_domain) {
  fqdn = strip_trailing_dot(fqdn);
  local_domain = strip_trailing_dot(local_domain);
  if (local_domain.empty())
    return fqdn;

  const size_t DOT = fqdn.find_first_of('.');
  if (DOT == 0 || DOT == cpp::string_view::npos)
    return fqdn;

  if (ascii_iequals(fqdn.substr(DOT + 1), local_domain))
    return fqdn.substr(0, DOT);
  return fqdn;
}

} // namespace netdb
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_NETDB_DNS_PTR_H
