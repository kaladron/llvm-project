//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Multi-nameserver DNS retry and rotation traversal driver.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_NETDB_DNS_TRANSPORT_H
#define LLVM_LIBC_SRC___SUPPORT_NETDB_DNS_TRANSPORT_H

#include "hdr/stdint_proxy.h"
#include "hdr/types/size_t.h"
#include "src/__support/CPP/span.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/netdb/dns_response.h"
#include "src/__support/netdb/resolv_conf.h"

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
};

} // namespace netdb
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_NETDB_DNS_TRANSPORT_H
