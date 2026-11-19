//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Numeric IPv4 and service-table socket address translator for getnameinfo.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_NETDB_NAMEINFO_H
#define LLVM_LIBC_SRC___SUPPORT_NETDB_NAMEINFO_H

#include "hdr/netdb_macros.h"
#include "hdr/stdint_proxy.h"
#include "hdr/sys_socket_macros.h"
#include "hdr/types/in_port_t.h"
#include "hdr/types/size_t.h"
#include "hdr/types/socklen_t.h"
#include "hdr/types/struct_in_addr.h"
#include "hdr/types/struct_servent.h"
#include "hdr/types/struct_sockaddr.h"
#include "hdr/types/struct_sockaddr_in.h"
#include "src/__support/CPP/span.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/CPP/stringstream.h"
#include "src/__support/integer_to_string.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/net/address.h"
#include "src/__support/net/byte_order.h"
#include "src/__support/netdb/services.h"
#include "src/string/memory_utils/inline_memcpy.h"

namespace LIBC_NAMESPACE_DECL {
namespace netdb {

LIBC_INLINE int copy_ni_str(cpp::span<char> out, cpp::string_view sv) {
  cpp::StringStream stream(out);
  stream << sv << cpp::StringStream::ENDS;
  return stream.overflow() ? EAI_OVERFLOW : 0;
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
  constexpr int VALID_FLAGS =
      NI_NUMERICHOST | NI_NUMERICSERV | NI_NOFQDN | NI_NAMEREQD | NI_DGRAM;
  constexpr int CONFLICT_FLAGS = NI_NUMERICHOST | NI_NAMEREQD;
  if ((flags & ~VALID_FLAGS) != 0 || (flags & CONFLICT_FLAGS) == CONFLICT_FLAGS)
    return EAI_BADFLAGS;

  const bool WANT_NODE = node != nullptr && nodelen > 0;
  const bool WANT_SERV = service != nullptr && servicelen > 0;
  if (!WANT_NODE && !WANT_SERV)
    return EAI_NONAME;

  if (salen < sizeof(struct sockaddr_in) || sa->sa_family != AF_INET)
    return EAI_FAMILY;

  struct sockaddr_in s4{};
  inline_memcpy(&s4, sa, sizeof(s4));

  if (WANT_NODE) {
    if ((flags & NI_NAMEREQD) != 0)
      return EAI_NONAME;
    if (!net::ipv4_to_str(s4.sin_addr, {node, nodelen}))
      return EAI_OVERFLOW;
  }

  if (WANT_SERV)
    return resolve_ni_serv(s4.sin_port, flags, {service, servicelen}, sys_err);

  return 0;
}

} // namespace netdb
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_NETDB_NAMEINFO_H
