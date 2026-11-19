//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// RFC 1035 DNS QNAME wire encoder, bounded pointer decoder, and host checks.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_NETDB_DNS_WIRE_H
#define LLVM_LIBC_SRC___SUPPORT_NETDB_DNS_WIRE_H

#include "hdr/stdint_proxy.h"
#include "hdr/types/size_t.h"
#include "src/__support/CPP/array.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/CPP/span.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/ctype_utils.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/netdb/hosts.h"

namespace LIBC_NAMESPACE_DECL {
namespace netdb {

constexpr size_t DNS_HEADER_LEN = 12;
constexpr size_t DNS_MAX_WIRE_NAME = 255;
constexpr size_t DNS_MAX_PTR_HOPS = 16;

struct EncodedQname {
  size_t wire_len = 0;
  bool is_fqdn = false;
};

struct DecodedWireName {
  size_t consumed = 0;
  size_t wire_len = 0;
};

LIBC_INLINE constexpr uint16_t read_u16_be(cpp::span<const uint8_t> buf,
                                           size_t off) {
  return static_cast<uint16_t>((static_cast<uint16_t>(buf[off]) << 8) |
                               buf[off + 1]);
}

LIBC_INLINE constexpr void write_u16_be(cpp::span<uint8_t> buf, size_t off,
                                        uint16_t val) {
  buf[off] = static_cast<uint8_t>(val >> 8);
  buf[off + 1] = static_cast<uint8_t>(val & 0xffu);
}

LIBC_INLINE bool is_valid_host_label(cpp::string_view label) {
  if (label.empty())
    return false;
  if (label.size() > DNS_MAX_LABEL)
    return false;
  if (!internal::isalnum(label.front()))
    return false;
  if (!internal::isalnum(label.back()))
    return false;

  for (char ch : label) {
    if (!internal::isalnum(ch)) {
      if (ch != '-')
        return false;
    }
  }
  return true;
}

LIBC_INLINE constexpr bool is_valid_wire_char(uint8_t ch) {
  if (ch <= 0x20)
    return false;
  if (ch == 0x7f)
    return false;
  return ch != '.';
}

LIBC_INLINE bool wire_names_equal(cpp::span<const uint8_t> a,
                                  cpp::span<const uint8_t> b) {
  if (a.size() != b.size())
    return false;

  for (size_t i = 0; i < a.size(); ++i) {
    const char CA = static_cast<char>(a[i]);
    const char CB = static_cast<char>(b[i]);
    if (internal::tolower(CA) != internal::tolower(CB))
      return false;
  }
  return true;
}

LIBC_INLINE cpp::optional<EncodedQname> encode_qname(cpp::string_view name,
                                                     cpp::span<uint8_t> out) {
  bool is_fqdn = false;
  if (name.size() > 1) {
    if (name.back() == '.') {
      is_fqdn = true;
      name = name.substr(0, name.size() - 1);
    }
  }

  size_t opos = 0;
  size_t pos = 0;
  while (true) {
    const size_t DOT = name.find_first_of('.', pos);
    const size_t END = DOT == cpp::string_view::npos ? name.size() : DOT;
    const cpp::string_view LABEL = name.substr(pos, END - pos);
    if (!is_valid_host_label(LABEL))
      return cpp::nullopt;
    if (out.size() - opos <= LABEL.size() + 1)
      return cpp::nullopt;

    out[opos++] = static_cast<uint8_t>(LABEL.size());
    for (char ch : LABEL)
      out[opos++] = static_cast<uint8_t>(ch);

    if (DOT == cpp::string_view::npos) {
      out[opos++] = 0;
      return EncodedQname{opos, is_fqdn};
    }
    pos = DOT + 1;
  }
}

LIBC_INLINE constexpr cpp::optional<DecodedWireName>
read_wire_name(cpp::span<const uint8_t> msg, size_t start_off, size_t limit,
               cpp::array<uint8_t, DNS_MAX_WIRE_NAME> &out) {
  size_t cur = start_off;
  size_t wlen = 0;
  size_t used = 0;
  size_t hops = 0;
  bool jumped = false;

  while (cur < limit) {
    const size_t LEN = msg[cur];
    if (LEN == 0) {
      out[wlen++] = 0;
      const size_t CONSUMED = jumped ? used : (cur + 1 - start_off);
      return DecodedWireName{CONSUMED, wlen};
    }

    if ((LEN & 0xc0u) == 0xc0u) {
      if (limit - cur < 2)
        return cpp::nullopt;
      const size_t TARGET = ((LEN & 0x3fu) << 8) | msg[cur + 1];
      if (TARGET < DNS_HEADER_LEN)
        return cpp::nullopt;
      if (TARGET >= cur)
        return cpp::nullopt;
      if (hops >= DNS_MAX_PTR_HOPS)
        return cpp::nullopt;
      ++hops;
      if (!jumped) {
        used = cur + 2 - start_off;
        jumped = true;
        limit = msg.size();
      }
      cur = TARGET;
      continue;
    }

    if ((LEN & 0xc0u) != 0)
      return cpp::nullopt;
    if (limit - (cur + 1) < LEN)
      return cpp::nullopt;
    if ((DNS_MAX_WIRE_NAME - 1) - wlen <= LEN)
      return cpp::nullopt;

    out[wlen++] = static_cast<uint8_t>(LEN);
    for (size_t i = 0; i < LEN; ++i) {
      const uint8_t CH = msg[cur + 1 + i];
      if (!is_valid_wire_char(CH))
        return cpp::nullopt;
      out[wlen++] = CH;
    }
    cur += 1 + LEN;
  }

  return cpp::nullopt;
}

} // namespace netdb
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_NETDB_DNS_WIRE_H
