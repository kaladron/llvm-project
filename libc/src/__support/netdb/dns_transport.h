//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Multi-nameserver, ndots, and search-domain DNS traversal driver.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_NETDB_DNS_TRANSPORT_H
#define LLVM_LIBC_SRC___SUPPORT_NETDB_DNS_TRANSPORT_H

#include "hdr/stdint_proxy.h"
#include "hdr/types/size_t.h"
#include "src/__support/CPP/array.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/CPP/span.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/netdb/dns_response.h"
#include "src/__support/netdb/dns_wire.h"
#include "src/__support/netdb/resolv_conf.h"
#include "src/string/memory_utils/inline_memcpy.h"

namespace LIBC_NAMESPACE_DECL {
namespace netdb {

using DnsNsQueryFn = DnsStatus (*)(void *ctx, const ScopedIpAddr &ns,
                                   cpp::span<const uint8_t> qname,
                                   uint16_t qtype, uint32_t timeout_ms,
                                   DnsParsedResponse &out);

LIBC_INLINE constexpr DnsStatus worse_miss(DnsStatus a, DnsStatus b) {
  return b > a ? b : a;
}

struct DnsResolver {
  DnsNsQueryFn query_ns;
  void *ctx;
  const ResolvConfig &cfg;
  size_t start_ns = 0;

  LIBC_INLINE DnsStatus query_qname(cpp::span<const uint8_t> qname,
                                    uint16_t qtype,
                                    DnsParsedResponse &out) const {
    const uint32_t TIMEOUT_MS = static_cast<uint32_t>(cfg.timeout) * 1000u;
    DnsStatus worst = DnsStatus::NXDOMAIN;

    for (uint8_t attempt = 0; attempt < cfg.attempts; ++attempt) {
      for (size_t s = 0; s < cfg.num_nameservers; ++s) {
        const size_t IDX = (start_ns + s) % cfg.num_nameservers;
        const DnsStatus ST =
            query_ns(ctx, cfg.nameservers[IDX], qname, qtype, TIMEOUT_MS, out);
        if (ST != DnsStatus::RETRY && ST != DnsStatus::FAIL)
          return ST;
        worst = worse_miss(worst, ST);
      }
    }

    return worst;
  }

  LIBC_INLINE DnsStatus walk_search(cpp::span<const uint8_t> bare_q,
                                    uint16_t qtype, DnsParsedResponse &out,
                                    DnsStatus &worst) const {
    cpp::array<uint8_t, DNS_MAX_WIRE_NAME> cand{};
    const size_t PREFIX_LEN = bare_q.size() - 1;

    for (size_t i = 0; i < cfg.num_search; ++i) {
      const size_t SUFFIX_LEN = cfg.search[i].len;
      if (cand.size() - PREFIX_LEN < SUFFIX_LEN)
        continue;

      inline_memcpy(cand.data(), bare_q.data(), PREFIX_LEN);
      inline_memcpy(cand.data() + PREFIX_LEN,
                    cfg.search_buf.data() + cfg.search[i].offset, SUFFIX_LEN);

      const DnsStatus ST =
          query_qname({cand.data(), PREFIX_LEN + SUFFIX_LEN}, qtype, out);
      if (ST >= DnsStatus::NODATA)
        return ST;
      worst = worse_miss(worst, ST);
    }

    return DnsStatus::NXDOMAIN;
  }

  LIBC_INLINE DnsStatus resolve_name(cpp::string_view name, uint16_t qtype,
                                     DnsParsedResponse &out) const {
    cpp::array<uint8_t, DNS_MAX_WIRE_NAME> bare{};
    const cpp::optional<EncodedQname> ENC = encode_qname(name, bare);
    if (!ENC.has_value())
      return DnsStatus::NXDOMAIN;

    const cpp::span<const uint8_t> BARE_Q(bare.data(), ENC->wire_len);
    if (ENC->is_fqdn || cfg.num_search == 0)
      return query_qname(BARE_Q, qtype, out);

    size_t dots = 0;
    for (char ch : name) {
      if (ch == '.')
        ++dots;
    }

    DnsStatus worst = DnsStatus::NXDOMAIN;
    const bool BARE_FIRST = dots >= cfg.ndots;
    for (int step = 0; step < 2; ++step) {
      const bool DO_BARE = (step == 0) == BARE_FIRST;
      const DnsStatus ST = DO_BARE ? query_qname(BARE_Q, qtype, out)
                                   : walk_search(BARE_Q, qtype, out, worst);
      if (ST >= DnsStatus::NODATA)
        return ST;
      worst = worse_miss(worst, ST);
    }

    return worst;
  }
};

} // namespace netdb
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_NETDB_DNS_TRANSPORT_H
