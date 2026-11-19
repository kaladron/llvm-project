//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// DNS RFC 5452 response validator and 8-hop unordered CNAME resolver.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_NETDB_DNS_RESPONSE_H
#define LLVM_LIBC_SRC___SUPPORT_NETDB_DNS_RESPONSE_H

#include "hdr/stdint_proxy.h"
#include "hdr/types/size_t.h"
#include "src/__support/CPP/array.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/CPP/span.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/netdb/dns_wire.h"
#include "src/string/memory_utils/inline_memcpy.h"

namespace LIBC_NAMESPACE_DECL {
namespace netdb {

constexpr uint16_t DNS_CLASS_IN = 1;
constexpr uint16_t DNS_TYPE_A = 1;
constexpr uint16_t DNS_TYPE_CNAME = 5;
constexpr uint16_t DNS_FLAG_QR = 0x8000;
constexpr uint16_t DNS_FLAG_OPCODE_MASK = 0x7800;
constexpr uint16_t DNS_FLAG_RD = 0x0100;
constexpr uint16_t DNS_RCODE_MASK = 0x000f;
constexpr uint16_t DNS_RCODE_SERVFAIL = 2;
constexpr uint16_t DNS_RCODE_NXDOMAIN = 3;
constexpr size_t DNS_MAX_CNAME_HOPS = 8;
constexpr size_t DNS_MAX_ADDRS = 16;

enum class DnsStatus : uint8_t {
  IGNORE,
  NXDOMAIN,
  FAIL,
  RETRY,
  NODATA,
  OK,
};

struct DnsParsedResponse {
  cpp::array<uint8_t, DNS_MAX_WIRE_NAME> canon_wire{};
  size_t clen = 0;
  size_t num_addrs = 0;
  cpp::array<cpp::array<uint8_t, 16>, DNS_MAX_ADDRS> addrs{};
};

LIBC_INLINE size_t match_question(cpp::span<const uint8_t> msg,
                                  cpp::span<const uint8_t> qname,
                                  uint16_t qtype) {
  cpp::array<uint8_t, DNS_MAX_WIRE_NAME> qbuf{};
  const cpp::optional<DecodedWireName> DEC =
      read_wire_name(msg, DNS_HEADER_LEN, msg.size(), qbuf);
  if (!DEC.has_value())
    return 0;

  const size_t END = DNS_HEADER_LEN + DEC->consumed;
  if (msg.size() - END < 4)
    return 0;
  if (read_u16_be(msg, END) != qtype)
    return 0;
  if (read_u16_be(msg, END + 2) != DNS_CLASS_IN)
    return 0;
  if (!wire_names_equal({qbuf.data(), DEC->wire_len}, qname))
    return 0;

  return END + 4;
}

LIBC_INLINE constexpr DnsStatus classify_rcode(uint16_t rcode) {
  if (rcode == DNS_RCODE_NXDOMAIN)
    return DnsStatus::NXDOMAIN;
  if (rcode == DNS_RCODE_SERVFAIL)
    return DnsStatus::RETRY;
  return DnsStatus::FAIL;
}

LIBC_INLINE DnsStatus parse_dns_response(cpp::span<const uint8_t> msg,
                                         uint16_t txid,
                                         cpp::span<const uint8_t> qname,
                                         uint16_t qtype,
                                         DnsParsedResponse &out) {
  if (msg.size() < DNS_HEADER_LEN)
    return DnsStatus::IGNORE;
  if (read_u16_be(msg, 0) != txid)
    return DnsStatus::IGNORE;

  const uint16_t FLAGS = read_u16_be(msg, 2);
  const uint16_t QDCOUNT = read_u16_be(msg, 4);
  const uint16_t RCODE = FLAGS & DNS_RCODE_MASK;

  if ((FLAGS & (DNS_FLAG_QR | DNS_FLAG_OPCODE_MASK)) != DNS_FLAG_QR)
    return DnsStatus::IGNORE;

  if (QDCOUNT != 1) {
    if (QDCOUNT == 0) {
      if (RCODE != 0 && RCODE != DNS_RCODE_NXDOMAIN)
        return classify_rcode(RCODE);
    }
    return DnsStatus::IGNORE;
  }

  const size_t OFF = match_question(msg, qname, qtype);
  if (OFF == 0)
    return DnsStatus::IGNORE;
  if (RCODE != 0)
    return classify_rcode(RCODE);

  out.clen = qname.size();
  inline_memcpy(out.canon_wire.data(), qname.data(), qname.size());

  const uint16_t ANCOUNT = read_u16_be(msg, 6);
  for (size_t hop = 0; hop <= DNS_MAX_CNAME_HOPS; ++hop) {
    cpp::array<uint8_t, DNS_MAX_WIRE_NAME> next_cn{};
    cpp::array<uint8_t, DNS_MAX_WIRE_NAME> owner{};
    cpp::array<uint8_t, DNS_MAX_WIRE_NAME> rdata_name{};
    size_t next_len = 0;
    size_t cur = OFF;
    bool found_cn = false;
    out.num_addrs = 0;

    for (uint16_t i = 0; i < ANCOUNT; ++i) {
      const cpp::optional<DecodedWireName> OWN =
          read_wire_name(msg, cur, msg.size(), owner);
      if (!OWN.has_value())
        return DnsStatus::IGNORE;

      const size_t HDR = cur + OWN->consumed;
      if (msg.size() - HDR < 10)
        return DnsStatus::IGNORE;

      const uint16_t RTYPE = read_u16_be(msg, HDR);
      const uint16_t RCLASS = read_u16_be(msg, HDR + 2);
      const uint16_t RDLEN = read_u16_be(msg, HDR + 8);
      if (msg.size() - (HDR + 10) < RDLEN)
        return DnsStatus::IGNORE;

      const size_t NEXT_RR = HDR + 10 + RDLEN;
      cur = NEXT_RR;
      if (RCLASS != DNS_CLASS_IN)
        continue;
      if (RTYPE == DNS_TYPE_A && RDLEN != 4)
        return DnsStatus::IGNORE;

      if (RTYPE == DNS_TYPE_CNAME) {
        const cpp::optional<DecodedWireName> TARGET =
            read_wire_name(msg, HDR + 10, NEXT_RR, rdata_name);
        if (!TARGET.has_value())
          return DnsStatus::IGNORE;
        if (TARGET->consumed != RDLEN)
          return DnsStatus::IGNORE;
        if (!found_cn) {
          if (wire_names_equal({owner.data(), OWN->wire_len},
                               {out.canon_wire.data(), out.clen})) {
            next_cn = rdata_name;
            next_len = TARGET->wire_len;
            found_cn = true;
          }
        }
      } else if (RTYPE == qtype) {
        if (out.num_addrs < DNS_MAX_ADDRS) {
          if (wire_names_equal({owner.data(), OWN->wire_len},
                               {out.canon_wire.data(), out.clen})) {
            inline_memcpy(out.addrs[out.num_addrs].data(),
                          msg.data() + HDR + 10, RDLEN);
            ++out.num_addrs;
          }
        }
      }
    }

    if (!found_cn)
      return out.num_addrs > 0 ? DnsStatus::OK : DnsStatus::NODATA;

    out.canon_wire = next_cn;
    out.clen = next_len;
  }

  return DnsStatus::FAIL;
}

} // namespace netdb
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_NETDB_DNS_RESPONSE_H
