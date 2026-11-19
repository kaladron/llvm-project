//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// DNS A query packet writer, response parser, and wire hook driver.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_NETDB_DNS_CLIENT_H
#define LLVM_LIBC_SRC___SUPPORT_NETDB_DNS_CLIENT_H

#include "hdr/stdint_proxy.h"
#include "hdr/types/size_t.h"
#include "hdr/types/struct_in_addr.h"
#include "src/__support/CPP/array.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/CPP/span.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/netdb/dns_wire.h"
#include "src/string/memory_utils/inline_memcpy.h"

namespace LIBC_NAMESPACE_DECL {
namespace netdb {

constexpr uint16_t DNS_CLASS_IN = 1;
constexpr uint16_t DNS_TYPE_A = 1;
constexpr uint16_t DNS_FLAG_QR = 0x8000;
constexpr uint16_t DNS_FLAG_OPCODE_MASK = 0x7800;
constexpr uint16_t DNS_FLAG_RD = 0x0100;
constexpr uint16_t DNS_RCODE_MASK = 0x000f;
constexpr size_t DNS_MAX_ADDRS = 16;
constexpr size_t DNS_UDP_RX_SIZE = 1232;
constexpr size_t DNS_MAX_QUERY_PKT = DNS_HEADER_LEN + DNS_MAX_WIRE_NAME + 4;

enum class DnsStatus : uint8_t { SERVER_FAILURE, NXDOMAIN, INVALID_NAME, OK };

using DnsWireHook = int (*)(const ScopedIpAddr &ns, bool is_tcp,
                            cpp::span<const uint8_t> query,
                            cpp::span<uint8_t> resp);

#ifndef LIBC_COPT_PUBLIC_PACKAGING
LIBC_INLINE_VAR LIBC_THREAD_LOCAL DnsWireHook dns_wire_hook = nullptr;

LIBC_INLINE void set_dns_wire_hook(DnsWireHook hook) { dns_wire_hook = hook; }

LIBC_INLINE DnsWireHook get_dns_wire_hook() { return dns_wire_hook; }
#else
LIBC_ALWAYS_INLINE constexpr DnsWireHook get_dns_wire_hook() { return nullptr; }
#endif // LIBC_COPT_PUBLIC_PACKAGING

LIBC_INLINE constexpr size_t write_query_packet(uint16_t txid,
                                                cpp::span<const uint8_t> qname,
                                                uint16_t qtype,
                                                cpp::span<uint8_t> out) {
  for (size_t i = 0; i < DNS_HEADER_LEN; ++i)
    out[i] = 0;

  write_u16_be(out, 0, txid);
  write_u16_be(out, 2, DNS_FLAG_RD);
  write_u16_be(out, 4, 1);

  for (size_t i = 0; i < qname.size(); ++i)
    out[DNS_HEADER_LEN + i] = qname[i];

  write_u16_be(out, DNS_HEADER_LEN + qname.size(), qtype);
  write_u16_be(out, DNS_HEADER_LEN + qname.size() + 2, DNS_CLASS_IN);
  return DNS_HEADER_LEN + qname.size() + 4;
}

LIBC_INLINE cpp::optional<size_t> match_question(cpp::span<const uint8_t> msg,
                                                 cpp::span<const uint8_t> qname,
                                                 uint16_t qtype) {
  cpp::array<uint8_t, DNS_MAX_WIRE_NAME> qbuf{};
  const cpp::optional<DecodedWireName> DEC =
      read_wire_name(msg, DNS_HEADER_LEN, msg.size(), qbuf);
  if (!DEC.has_value())
    return cpp::nullopt;

  const size_t END = DNS_HEADER_LEN + DEC->consumed;
  if (msg.size() - END < 4)
    return cpp::nullopt;
  if (read_u16_be(msg, END) != qtype)
    return cpp::nullopt;
  if (read_u16_be(msg, END + 2) != DNS_CLASS_IN)
    return cpp::nullopt;
  if (!wire_names_equal({qbuf.data(), DEC->wire_len}, qname))
    return cpp::nullopt;

  return END + 4;
}

LIBC_INLINE DnsStatus parse_a_response(cpp::span<const uint8_t> msg,
                                       uint16_t txid,
                                       cpp::span<const uint8_t> qname,
                                       cpp::span<struct in_addr> addrs,
                                       size_t &num_addrs) {
  num_addrs = 0;
  if (msg.size() < DNS_HEADER_LEN)
    return DnsStatus::SERVER_FAILURE;
  if (read_u16_be(msg, 0) != txid)
    return DnsStatus::SERVER_FAILURE;

  const uint16_t FLAGS = read_u16_be(msg, 2);
  if ((FLAGS & (DNS_FLAG_QR | DNS_FLAG_OPCODE_MASK)) != DNS_FLAG_QR)
    return DnsStatus::SERVER_FAILURE;
  if (read_u16_be(msg, 4) != 1)
    return DnsStatus::SERVER_FAILURE;

  const cpp::optional<size_t> OFF = match_question(msg, qname, DNS_TYPE_A);
  if (!OFF.has_value())
    return DnsStatus::SERVER_FAILURE;
  if ((FLAGS & DNS_RCODE_MASK) != 0)
    return DnsStatus::NXDOMAIN;

  size_t cur = *OFF;
  const uint16_t ANCOUNT = read_u16_be(msg, 6);
  for (uint16_t i = 0; i < ANCOUNT; ++i) {
    cpp::array<uint8_t, DNS_MAX_WIRE_NAME> owner{};
    const cpp::optional<DecodedWireName> OWN =
        read_wire_name(msg, cur, msg.size(), owner);
    if (!OWN.has_value())
      return DnsStatus::SERVER_FAILURE;

    const size_t HDR = cur + OWN->consumed;
    if (msg.size() - HDR < 10)
      return DnsStatus::SERVER_FAILURE;

    const uint16_t RDLEN = read_u16_be(msg, HDR + 8);
    if (msg.size() - (HDR + 10) < RDLEN)
      return DnsStatus::SERVER_FAILURE;

    if (read_u16_be(msg, HDR + 2) == DNS_CLASS_IN &&
        read_u16_be(msg, HDR) == DNS_TYPE_A) {
      if (RDLEN != 4)
        return DnsStatus::SERVER_FAILURE;
      if (wire_names_equal({owner.data(), OWN->wire_len}, qname)) {
        if (num_addrs < addrs.size())
          inline_memcpy(&addrs[num_addrs++], msg.data() + HDR + 10, 4);
      }
    }
    cur = HDR + 10 + RDLEN;
  }

  return num_addrs > 0 ? DnsStatus::OK : DnsStatus::NXDOMAIN;
}

LIBC_INLINE DnsStatus query_dns_a(cpp::string_view name,
                                  cpp::span<struct in_addr> addrs,
                                  size_t &num_addrs) {
  cpp::array<uint8_t, DNS_MAX_WIRE_NAME> qwire{};
  const cpp::optional<EncodedQname> ENC = encode_qname(name, qwire);
  if (!ENC.has_value())
    return DnsStatus::INVALID_NAME;

  const DnsWireHook HOOK = get_dns_wire_hook();
  if (HOOK == nullptr)
    return DnsStatus::SERVER_FAILURE;

  const cpp::span<const uint8_t> QNAME(qwire.data(), ENC->wire_len);
  cpp::array<uint8_t, DNS_MAX_QUERY_PKT> tx{};
  cpp::array<uint8_t, DNS_UDP_RX_SIZE> rx{};
  constexpr uint16_t INITIAL_TXID = 0x0101;
  const size_t QLEN = write_query_packet(INITIAL_TXID, QNAME, DNS_TYPE_A, tx);

  ScopedIpAddr lo{};
  lo.addr[0] = 127;
  lo.addr[3] = 1;
  const int N = HOOK(lo, false, {tx.data(), QLEN}, rx);
  if (N < 0)
    return DnsStatus::SERVER_FAILURE;

  return parse_a_response({rx.data(), static_cast<size_t>(N)}, INITIAL_TXID,
                          QNAME, addrs, num_addrs);
}

} // namespace netdb
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_NETDB_DNS_CLIENT_H
