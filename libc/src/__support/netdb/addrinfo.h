//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Co-allocated struct addrinfo builder and local literal getaddrinfo driver.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_NETDB_ADDRINFO_H
#define LLVM_LIBC_SRC___SUPPORT_NETDB_ADDRINFO_H

#include "hdr/netdb_macros.h"
#include "hdr/netinet_in_macros.h"
#include "hdr/stdint_proxy.h"
#include "hdr/sys_socket_macros.h"
#include "hdr/types/in_port_t.h"
#include "hdr/types/size_t.h"
#include "hdr/types/struct_addrinfo.h"
#include "hdr/types/struct_sockaddr.h"
#include "hdr/types/struct_sockaddr_in.h"
#include "hdr/types/struct_sockaddr_in6.h"
#include "src/__support/CPP/array.h"
#include "src/__support/CPP/functional.h"
#include "src/__support/CPP/new.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/CPP/span.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/CPP/unique_ptr.h"
#include "src/__support/checked_ptr.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/net/byte_order.h"
#include "src/__support/netdb/hosts.h"
#include "src/__support/netdb/ip_parse.h"
#include "src/string/memory_utils/inline_memcpy.h"

namespace LIBC_NAMESPACE_DECL {
namespace netdb {

struct AddrinfoNode {
  struct addrinfo ai{};
  union {
    struct sockaddr_in v4;
    struct sockaddr_in6 v6;
  } sa{};
};
static_assert(__builtin_offsetof(AddrinfoNode, ai) == 0);

LIBC_INLINE void free_addrinfo_list(struct addrinfo *ai) {
  while (ai != nullptr) {
    struct addrinfo *next = ai->ai_next;
    delete[] reinterpret_cast<uint8_t *>(ai);
    ai = next;
  }
}

struct AddrinfoDeleter {
  LIBC_INLINE void operator()(struct addrinfo *ai) const {
    free_addrinfo_list(ai);
  }
};

using AddrinfoPtr = cpp::unique_ptr<struct addrinfo, AddrinfoDeleter>;
using CheckedAddrInfo = CheckedPtr<struct addrinfo, AddrinfoDeleter>;

struct ServiceSpec {
  int socktype = 0;
  int protocol = 0;
  in_port_t net_port = 0;
};

using ServLookupFn =
    cpp::function_ref<int(cpp::string_view, const char *, int, int,
                          cpp::span<ServiceSpec>, size_t &, int &)>;

#ifndef LIBC_COPT_PUBLIC_PACKAGING
LIBC_INLINE_VAR LIBC_THREAD_LOCAL bool ai_alloc_budget_active = false;
LIBC_INLINE_VAR LIBC_THREAD_LOCAL size_t ai_alloc_budget = 0;

LIBC_INLINE void set_addrinfo_alloc_budget(size_t budget) {
  ai_alloc_budget_active = true;
  ai_alloc_budget = budget;
}

LIBC_INLINE void clear_addrinfo_alloc_budget() {
  ai_alloc_budget_active = false;
  ai_alloc_budget = 0;
}

LIBC_INLINE CheckedPtr<uint8_t[]> alloc_ai_bytes(size_t n) {
  if (ai_alloc_budget_active) {
    if (ai_alloc_budget == 0)
      return nullptr;
    --ai_alloc_budget;
  }
  return make_unique<uint8_t[]>(n);
}
#else
LIBC_ALWAYS_INLINE CheckedPtr<uint8_t[]> alloc_ai_bytes(size_t n) {
  return make_unique<uint8_t[]>(n);
}
#endif // LIBC_COPT_PUBLIC_PACKAGING

LIBC_INLINE constexpr ScopedIpAddr to_v4mapped_ip6(const ScopedIpAddr &v4) {
  ScopedIpAddr out{};
  out.is_ipv6 = true;
  out.addr[10] = 0xff;
  out.addr[11] = 0xff;
  for (size_t i = 0; i < 4; ++i)
    out.addr[12 + i] = v4.addr[i];
  return out;
}

LIBC_INLINE CheckedAddrInfo alloc_ai_node(const ScopedIpAddr &ip,
                                          ServiceSpec spec, int flags,
                                          cpp::string_view canonname) {
  const size_t EXTRA = canonname.empty() ? 0 : canonname.size() + 1;
  const size_t TOTAL = sizeof(AddrinfoNode) + EXTRA;
  CheckedPtr<uint8_t[]> raw = alloc_ai_bytes(TOTAL);
  if (!raw)
    return nullptr;

  uint8_t *buf = raw.get();
  auto *node = new (buf) AddrinfoNode{};
  node->ai.ai_flags = flags;
  node->ai.ai_socktype = spec.socktype;
  node->ai.ai_protocol = spec.protocol;
  node->ai.ai_addr = reinterpret_cast<struct sockaddr *>(&node->sa);

  if (ip.is_ipv6) {
    node->ai.ai_family = AF_INET6;
    node->ai.ai_addrlen = sizeof(node->sa.v6);
    node->sa.v6.sin6_family = AF_INET6;
    node->sa.v6.sin6_port = spec.net_port;
    node->sa.v6.sin6_scope_id = ip.scope_id;
    inline_memcpy(&node->sa.v6.sin6_addr, ip.addr.data(),
                  sizeof(node->sa.v6.sin6_addr));
  } else {
    node->ai.ai_family = AF_INET;
    node->ai.ai_addrlen = sizeof(node->sa.v4);
    node->sa.v4.sin_family = AF_INET;
    node->sa.v4.sin_port = spec.net_port;
    inline_memcpy(&node->sa.v4.sin_addr, ip.addr.data(),
                  sizeof(node->sa.v4.sin_addr));
  }

  if (EXTRA > 0) {
    cpp::span<uint8_t> tail =
        cpp::span<uint8_t>(buf, TOTAL).subspan(sizeof(AddrinfoNode));
    char *dst = reinterpret_cast<char *>(tail.data());
    inline_memcpy(dst, canonname.data(), canonname.size());
    dst[canonname.size()] = '\0';
    node->ai.ai_canonname = dst;
  }

  (void)raw.release();
  return CheckedAddrInfo(&node->ai);
}

LIBC_INLINE int emit_ai_list(cpp::span<const ScopedIpAddr> addrs,
                             cpp::span<const ServiceSpec> specs, int flags,
                             cpp::string_view canonname,
                             struct addrinfo **res) {
  AddrinfoPtr head;
  struct addrinfo **tail = nullptr;

  for (const ScopedIpAddr &IP : addrs) {
    for (const ServiceSpec &SPEC : specs) {
      CheckedAddrInfo node = alloc_ai_node(IP, SPEC, flags, canonname);
      if (!node)
        return EAI_MEMORY;

      struct addrinfo *raw = node.release();
      if (!head)
        head.reset(raw);
      else
        *tail = raw;

      tail = &raw->ai_next;
      canonname = {};
    }
  }

  *res = head.release();
  return 0;
}

LIBC_INLINE constexpr bool matches_hint(int hint_val, int target_val) {
  return hint_val == 0 || hint_val == target_val;
}

LIBC_INLINE constexpr void push_tcp_udp(bool allow_tcp, bool allow_udp,
                                        in_port_t net_port,
                                        cpp::span<ServiceSpec> specs,
                                        size_t &count) {
  if (allow_tcp)
    specs[count++] = {SOCK_STREAM, IPPROTO_TCP, net_port};
  if (allow_udp)
    specs[count++] = {SOCK_DGRAM, IPPROTO_UDP, net_port};
}

LIBC_INLINE int resolve_services(const char *serv, int flags, int socktype,
                                 int protocol, ServLookupFn on_serv,
                                 cpp::span<ServiceSpec> specs, size_t &count,
                                 int &sys_err) {
  if (socktype == SOCK_RAW) {
    if (protocol < 0)
      return EAI_SOCKTYPE;
    if (serv != nullptr)
      return EAI_SERVICE;
    specs[count++] = {SOCK_RAW, protocol, 0};
    return 0;
  }

  const bool ALLOW_TCP = matches_hint(socktype, SOCK_STREAM) &&
                         matches_hint(protocol, IPPROTO_TCP);
  const bool ALLOW_UDP =
      matches_hint(socktype, SOCK_DGRAM) && matches_hint(protocol, IPPROTO_UDP);
  if (!ALLOW_TCP && !ALLOW_UDP)
    return EAI_SOCKTYPE;

  if (serv != nullptr) {
    const cpp::optional<uint16_t> PORT = parse_unsigned<uint16_t>(serv);
    if (PORT.has_value()) {
      push_tcp_udp(ALLOW_TCP, ALLOW_UDP,
                   net::hton(static_cast<in_port_t>(*PORT)), specs, count);
      return 0;
    }
    if ((flags & AI_NUMERICSERV) != 0)
      return EAI_NONAME;

    struct ProtoCandidate {
      bool enabled;
      const char *name;
      int socktype;
      int protocol;
    };
    const ProtoCandidate CANDIDATES[2] = {
        {ALLOW_TCP, "tcp", SOCK_STREAM, IPPROTO_TCP},
        {ALLOW_UDP, "udp", SOCK_DGRAM, IPPROTO_UDP},
    };
    for (const ProtoCandidate &CAND : CANDIDATES) {
      if (!CAND.enabled)
        continue;
      if (on_serv(serv, CAND.name, CAND.socktype, CAND.protocol, specs, count,
                  sys_err) != 0)
        return EAI_SYSTEM;
    }
    return count > 0 ? 0 : EAI_SERVICE;
  }

  push_tcp_udp(ALLOW_TCP, ALLOW_UDP, 0, specs, count);
  if (socktype == 0 && protocol == 0)
    specs[count++] = {SOCK_RAW, 0, 0};
  return 0;
}

LIBC_INLINE constexpr bool has_ipv6_addr(const HostsForwardResult &res) {
  for (size_t i = 0; i < res.num_addrs; ++i) {
    if (res.addrs[i].is_ipv6)
      return true;
  }
  return false;
}

LIBC_INLINE constexpr void apply_v4mapped_hosts(HostsForwardResult &res,
                                                int flags) {
  bool keep_v4 = !has_ipv6_addr(res);
  if ((flags & AI_ALL) != 0)
    keep_v4 = true;

  size_t out_idx = 0;
  for (size_t i = 0; i < res.num_addrs; ++i) {
    if (res.addrs[i].is_ipv6) {
      res.addrs[out_idx++] = res.addrs[i];
    } else if (keep_v4) {
      res.addrs[out_idx++] = to_v4mapped_ip6(res.addrs[i]);
    }
  }
  res.num_addrs = out_idx;
}

LIBC_INLINE constexpr HostsFamily select_hosts_family(int family,
                                                      bool v4_mapped) {
  if (family == AF_UNSPEC)
    return HostsFamily::UNSPEC;
  if (family == AF_INET)
    return HostsFamily::IPV4;
  if (v4_mapped)
    return HostsFamily::UNSPEC;
  return HostsFamily::IPV6;
}

LIBC_INLINE int resolve_hosts_node(cpp::string_view node, int family, int flags,
                                   cpp::span<const ServiceSpec> specs,
                                   struct addrinfo **res, int &sys_err) {
  if ((flags & AI_NUMERICHOST) != 0)
    return EAI_NONAME;

  bool v4_mapped = false;
  if (family == AF_INET6)
    v4_mapped = (flags & AI_V4MAPPED) != 0;

  HostsForwardResult hosts_res{};
  ErrorOr<bool> found_or =
      lookup_hosts_forward(node, select_hosts_family(family, v4_mapped),
                           hosts_res, get_hosts_path());
  if (!found_or.has_value()) {
    sys_err = found_or.error();
    return EAI_SYSTEM;
  }
  if (!*found_or)
    return EAI_NONAME;

  if (v4_mapped)
    apply_v4mapped_hosts(hosts_res, flags);

  cpp::string_view canonname{};
  if ((flags & AI_CANONNAME) != 0)
    canonname = {hosts_res.canon_name.data(), hosts_res.canon_len};

  return emit_ai_list({hosts_res.addrs.data(), hosts_res.num_addrs}, specs,
                      flags, canonname, res);
}

LIBC_INLINE constexpr bool valid_ai_family(int family) {
  return family == AF_UNSPEC || family == AF_INET || family == AF_INET6;
}

LIBC_INLINE int lookup_addrinfo(const char *node, const char *serv,
                                const struct addrinfo *hints,
                                ServLookupFn on_serv, struct addrinfo **res,
                                int &sys_err) {
  *res = nullptr;

  const struct addrinfo DEFAULT_HINTS{};
  const struct addrinfo &H = hints != nullptr ? *hints : DEFAULT_HINTS;

  constexpr int VALID_FLAGS = AI_PASSIVE | AI_CANONNAME | AI_NUMERICHOST |
                              AI_V4MAPPED | AI_ALL | AI_ADDRCONFIG |
                              AI_NUMERICSERV;
  if ((H.ai_flags & ~VALID_FLAGS) != 0)
    return EAI_BADFLAGS;
  if (!valid_ai_family(H.ai_family))
    return EAI_FAMILY;

  if (node == nullptr) {
    if (serv == nullptr)
      return EAI_NONAME;
    if ((H.ai_flags & AI_CANONNAME) != 0)
      return EAI_BADFLAGS;
  }

  cpp::array<ServiceSpec, 3> specs{};
  size_t spec_count = 0;
  const int SERV_RC =
      resolve_services(serv, H.ai_flags, H.ai_socktype, H.ai_protocol, on_serv,
                       specs, spec_count, sys_err);
  if (SERV_RC != 0)
    return SERV_RC;

  const cpp::span<const ServiceSpec> ACTIVE_SPECS(specs.data(), spec_count);

  if (node == nullptr) {
    ScopedIpAddr addrs[2]{};
    addrs[1].is_ipv6 = true;
    if ((H.ai_flags & AI_PASSIVE) == 0) {
      addrs[0].addr[0] = 127;
      addrs[0].addr[3] = 1;
      addrs[1].addr[15] = 1;
    }

    const size_t OFFSET = H.ai_family == AF_INET6 ? 1 : 0;
    const size_t ADDR_COUNT = H.ai_family == AF_UNSPEC ? 2 : 1;
    return emit_ai_list(
        cpp::span<const ScopedIpAddr>(addrs).subspan(OFFSET, ADDR_COUNT),
        ACTIVE_SPECS, H.ai_flags, {}, res);
  }

  ScopedIpAddr ip{};
  if (!parse_scoped_ip(node, ip))
    return resolve_hosts_node(node, H.ai_family, H.ai_flags, ACTIVE_SPECS, res,
                              sys_err);

  if (!ip.is_ipv6) {
    if (H.ai_family == AF_INET6) {
      if ((H.ai_flags & AI_V4MAPPED) == 0)
        return EAI_NONAME;
      ip = to_v4mapped_ip6(ip);
    }
  } else if (H.ai_family == AF_INET) {
    return EAI_NONAME;
  }

  cpp::string_view canonname{};
  if ((H.ai_flags & AI_CANONNAME) != 0)
    canonname = node;

  return emit_ai_list({&ip, 1}, ACTIVE_SPECS, H.ai_flags, canonname, res);
}

} // namespace netdb
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_NETDB_ADDRINFO_H
