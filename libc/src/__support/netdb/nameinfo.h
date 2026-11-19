//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Socket address to host and service translator for getnameinfo.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_NETDB_NAMEINFO_H
#define LLVM_LIBC_SRC___SUPPORT_NETDB_NAMEINFO_H

#include "hdr/net_if_macros.h"
#include "hdr/netdb_macros.h"
#include "hdr/stdint_proxy.h"
#include "hdr/sys_socket_macros.h"
#include "hdr/types/in_port_t.h"
#include "hdr/types/size_t.h"
#include "hdr/types/socklen_t.h"
#include "hdr/types/struct_in6_addr.h"
#include "hdr/types/struct_in_addr.h"
#include "hdr/types/struct_servent.h"
#include "hdr/types/struct_sockaddr.h"
#include "hdr/types/struct_sockaddr_in.h"
#include "hdr/types/struct_sockaddr_in6.h"
#include "src/__support/CPP/array.h"
#include "src/__support/CPP/span.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/CPP/stringstream.h"
#include "src/__support/OSUtil/network.h"
#include "src/__support/error_or.h"
#include "src/__support/integer_to_string.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/net/address.h"
#include "src/__support/net/byte_order.h"
#include "src/__support/netdb/dns_client.h"
#include "src/__support/netdb/dns_ptr.h"
#include "src/__support/netdb/dns_response.h"
#include "src/__support/netdb/dns_wire.h"
#include "src/__support/netdb/hosts.h"
#include "src/__support/netdb/hosts_reverse.h"
#include "src/__support/netdb/ip_parse.h"
#include "src/__support/netdb/resolv_conf.h"
#include "src/__support/netdb/services.h"
#include "src/string/memory_utils/inline_memcpy.h"

namespace LIBC_NAMESPACE_DECL {
namespace netdb {

LIBC_INLINE int copy_ni_str(cpp::span<char> out, cpp::string_view sv) {
  cpp::StringStream stream(out);
  stream << sv << cpp::StringStream::ENDS;
  return stream.overflow() ? EAI_OVERFLOW : 0;
}

LIBC_INLINE int copy_resolved_host(cpp::string_view host, int flags,
                                   const ResolvConfig &cfg,
                                   cpp::span<char> out) {
  if ((flags & NI_NOFQDN) != 0) {
    cpp::array<char, DNS_MAX_TEXT_NAME + 1> dom{};
    size_t dom_len = 0;
    if (cfg.num_search > 0) {
      dom_len = wire_to_presentation(
          {cfg.search_buf.data() + cfg.search[0].offset, cfg.search[0].len},
          dom);
    }
    host = apply_ni_nofqdn(host, {dom.data(), dom_len});
  }
  return copy_ni_str(out, host);
}

LIBC_INLINE bool extract_ni_target(const struct sockaddr *sa, socklen_t salen,
                                   ScopedIpAddr &ip, in_port_t &net_port) {
  if (salen < sizeof(struct sockaddr_in))
    return false;

  if (sa->sa_family == AF_INET) {
    struct sockaddr_in s4{};
    inline_memcpy(&s4, sa, sizeof(s4));
    inline_memcpy(ip.addr.data(), &s4.sin_addr, sizeof(s4.sin_addr));
    net_port = s4.sin_port;
    return true;
  }

  if (sa->sa_family != AF_INET6)
    return false;
  if (salen < sizeof(struct sockaddr_in6))
    return false;

  struct sockaddr_in6 s6{};
  inline_memcpy(&s6, sa, sizeof(s6));
  net_port = s6.sin6_port;

  const cpp::span<const uint8_t> BYTES(s6.sin6_addr.s6_addr, 16);
  if (is_v4_embedded_in6(BYTES)) {
    inline_memcpy(ip.addr.data(), BYTES.subspan(12, 4).data(), 4);
    return true;
  }

  inline_memcpy(ip.addr.data(), BYTES.data(), 16);
  ip.is_ipv6 = true;
  ip.scope_id = s6.sin6_scope_id;
  return true;
}

LIBC_INLINE int format_numeric_node(const ScopedIpAddr &ip, int flags,
                                    cpp::span<char> out) {
  if (!ip.is_ipv6) {
    struct in_addr v4{};
    inline_memcpy(&v4, ip.addr.data(), sizeof(v4));
    return net::ipv4_to_str(v4, out) ? 0 : EAI_OVERFLOW;
  }

  struct in6_addr v6{};
  inline_memcpy(&v6, ip.addr.data(), sizeof(v6));
  if (ip.scope_id == 0)
    return net::ipv6_to_str(v6, out) ? 0 : EAI_OVERFLOW;

  constexpr size_t IPV6_TEXT_MAX = 46;
  constexpr size_t SCOPED_TEXT_MAX = 64;
  char v6_buf[IPV6_TEXT_MAX]{};
  (void)net::ipv6_to_str(v6, v6_buf);

  char scoped_buf[SCOPED_TEXT_MAX];
  cpp::StringStream stream(scoped_buf);
  stream << cpp::string_view(v6_buf) << '%';

  char ifname[IF_NAMESIZE]{};
  if ((flags & NI_NUMERICSCOPE) == 0 &&
      network::if_indextoname(ip.scope_id, ifname)) {
    stream << cpp::string_view(ifname);
  } else {
    stream << ip.scope_id;
  }

  return copy_ni_str(out, stream.str());
}

LIBC_INLINE bool should_lookup_ni_host(const ScopedIpAddr &ip, int flags) {
  if ((flags & NI_NUMERICHOST) != 0)
    return false;
  if (ip.is_ipv6 && all_zero_bytes(ip.addr))
    return false;
  return true;
}

LIBC_INLINE int resolve_ni_node(const ScopedIpAddr &ip, int flags,
                                cpp::span<char> out, int &sys_err) {
  if (should_lookup_ni_host(ip, flags)) {
    HostsReverseResult hr{};
    const size_t ADDR_LEN = ip.is_ipv6 ? 16 : 4;
    ErrorOr<bool> hit =
        lookup_hosts_reverse(ip.is_ipv6, {ip.addr.data(), ADDR_LEN},
                             ip.scope_id, hr, get_hosts_path());
    if (!hit.has_value()) {
      sys_err = hit.error();
      return EAI_SYSTEM;
    }
    if (*hit && (flags & NI_NOFQDN) == 0)
      return copy_ni_str(out, {hr.canon_name.data(), hr.canon_len});

    ResolvConfig cfg{};
    ErrorOr<void> rcfg = load_resolv_conf(cfg, get_resolv_conf_path());
    if (!rcfg.has_value()) {
      sys_err = rcfg.error();
      return EAI_SYSTEM;
    }
    if (*hit)
      return copy_resolved_host({hr.canon_name.data(), hr.canon_len}, flags,
                                cfg, out);

    cpp::array<uint8_t, IPV6_PTR_WIRE_LEN> ptr_qname{};
    const size_t QLEN =
        ip.is_ipv6 ? build_ipv6_ptr_qname({ip.addr.data(), 16}, ptr_qname)
                   : build_ipv4_ptr_qname({ip.addr.data(), 4}, ptr_qname);
    DnsParsedResponse dns{};
    const DnsStatus ST =
        dispatch_dns_qname(cfg, {ptr_qname.data(), QLEN}, DNS_TYPE_PTR, dns);
    if (ST == DnsStatus::RETRY)
      return EAI_AGAIN;
    if (ST == DnsStatus::FAIL)
      return EAI_FAIL;
    if (ST == DnsStatus::OK) {
      cpp::array<char, DNS_MAX_TEXT_NAME + 1> host_buf{};
      const size_t HOST_LEN =
          wire_to_presentation({dns.canon_wire.data(), dns.clen}, host_buf);
      const cpp::string_view HOST(host_buf.data(), HOST_LEN);
      if (is_valid_ptr_hostname(HOST))
        return copy_resolved_host(HOST, flags, cfg, out);
    }
  }

  if ((flags & NI_NAMEREQD) != 0)
    return EAI_NONAME;
  return format_numeric_node(ip, flags, out);
}

LIBC_INLINE int resolve_ni_serv(in_port_t net_port, int flags,
                                cpp::span<char> out, int &sys_err) {
  if ((flags & NI_NUMERICSERV) == 0) {
    constexpr size_t SERV_BUF_SIZE = 1024;
    struct servent entry{};
    struct servent *found = nullptr;
    char buffer[SERV_BUF_SIZE];
    const char *proto = (flags & NI_DGRAM) != 0 ? "udp" : "tcp";
    const int RC =
        lookup_serv_by_port(static_cast<int>(net_port), proto, &entry, buffer,
                            &found, get_services_path());
    if (RC != 0) {
      sys_err = RC;
      return EAI_SYSTEM;
    }
    if (found != nullptr)
      return copy_ni_str(out, entry.s_name);
  }

  const IntegerToString<uint32_t> DEC(net::ntoh(net_port));
  return copy_ni_str(out, DEC.view());
}

LIBC_INLINE int lookup_nameinfo(const struct sockaddr *sa, socklen_t salen,
                                char *node, socklen_t nodelen, char *service,
                                socklen_t servicelen, int flags, int &sys_err) {
  constexpr int VALID_FLAGS = NI_NUMERICHOST | NI_NUMERICSERV | NI_NOFQDN |
                              NI_NAMEREQD | NI_DGRAM | NI_NUMERICSCOPE;
  constexpr int CONFLICT_FLAGS = NI_NUMERICHOST | NI_NAMEREQD;
  if ((flags & ~VALID_FLAGS) != 0 || (flags & CONFLICT_FLAGS) == CONFLICT_FLAGS)
    return EAI_BADFLAGS;

  const bool WANT_NODE = node != nullptr && nodelen > 0;
  const bool WANT_SERV = service != nullptr && servicelen > 0;
  if (!WANT_NODE && !WANT_SERV)
    return EAI_NONAME;

  ScopedIpAddr ip{};
  in_port_t net_port = 0;
  if (!extract_ni_target(sa, salen, ip, net_port))
    return EAI_FAMILY;

  if (WANT_NODE) {
    const int NODE_RC = resolve_ni_node(ip, flags, {node, nodelen}, sys_err);
    if (NODE_RC != 0)
      return NODE_RC;
  }

  if (WANT_SERV)
    return resolve_ni_serv(net_port, flags, {service, servicelen}, sys_err);

  return 0;
}

} // namespace netdb
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_NETDB_NAMEINFO_H
