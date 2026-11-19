//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Internal Linux network interface and Netlink address configuration helpers.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_NETWORK_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_NETWORK_H

#include "hdr/net_if_macros.h"
#include "hdr/stdint_proxy.h"
#include "hdr/sys_ioctl_macros.h"
#include "hdr/sys_socket_macros.h"
#include "hdr/types/size_t.h"
#include "hdr/types/ssize_t.h"
#include "hdr/types/struct_ifreq.h"
#include "src/__support/CPP/span.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/close.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/ioctl.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/recvfrom.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/sendto.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/socket.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/string/memory_utils/inline_memcpy.h"

#include <linux/netlink.h>
#include <linux/rtnetlink.h>

namespace LIBC_NAMESPACE_DECL {
namespace network {

LIBC_INLINE uint32_t if_nametoindex(cpp::string_view name,
                                    int sock_domain = AF_UNIX) {
  if (name.empty())
    return 0;
  if (name.size() >= IF_NAMESIZE)
    return 0;

  ErrorOr<int> fd =
      linux_syscalls::socket(sock_domain, SOCK_DGRAM | SOCK_CLOEXEC, 0);
  if (!fd.has_value())
    return 0;

  struct ifreq ifr{};
  inline_memcpy(ifr.ifr_name, name.data(), name.size());
  ErrorOr<int> rc = linux_syscalls::ioctl(*fd, SIOCGIFINDEX, &ifr);
  (void)linux_syscalls::close(*fd);
  if (!rc.has_value())
    return 0;

  return static_cast<uint32_t>(ifr.ifr_ifindex);
}

LIBC_INLINE constexpr size_t align_nl(size_t len, size_t align) {
  return (len + align - 1) & ~(align - 1);
}

LIBC_INLINE constexpr bool is_non_loopback_v4(cpp::span<const uint8_t> addr) {
  if (addr.size() != 4)
    return false;
  if (addr[0] == 0)
    return false;
  return addr[0] != 127;
}

LIBC_INLINE constexpr bool is_non_loopback_v6(cpp::span<const uint8_t> addr) {
  if (addr.size() != 16)
    return false;
  for (size_t i = 0; i < 15; ++i) {
    if (addr[i] != 0)
      return true;
  }
  return addr[15] > 1;
}

LIBC_INLINE void parse_addrconfig_rta(uint8_t family,
                                      cpp::span<const uint8_t> attrs,
                                      bool &has_v4, bool &has_v6) {
  while (attrs.size() >= sizeof(struct rtattr)) {
    struct rtattr rta{};
    inline_memcpy(&rta, attrs.data(), sizeof(rta));
    if (rta.rta_len < sizeof(struct rtattr))
      break;
    if (rta.rta_len > attrs.size())
      break;

    if (rta.rta_type == IFA_ADDRESS) {
      const cpp::span<const uint8_t> ADDR = attrs.subspan(
          sizeof(struct rtattr), rta.rta_len - sizeof(struct rtattr));
      if (family == AF_INET) {
        if (is_non_loopback_v4(ADDR))
          has_v4 = true;
      } else if (family == AF_INET6) {
        if (is_non_loopback_v6(ADDR))
          has_v6 = true;
      }
    }

    const size_t STEP = align_nl(rta.rta_len, RTA_ALIGNTO);
    attrs = attrs.subspan(STEP < attrs.size() ? STEP : attrs.size());
  }
}

LIBC_INLINE bool parse_addrconfig_batch(cpp::span<const uint8_t> raw,
                                        bool &has_v4, bool &has_v6) {
  constexpr size_t IFA_HDR_END =
      sizeof(struct nlmsghdr) + sizeof(struct ifaddrmsg);

  while (raw.size() >= sizeof(struct nlmsghdr)) {
    struct nlmsghdr nh{};
    inline_memcpy(&nh, raw.data(), sizeof(nh));
    if (nh.nlmsg_len < sizeof(struct nlmsghdr))
      break;
    if (nh.nlmsg_len > raw.size())
      break;
    if (nh.nlmsg_type == NLMSG_DONE)
      return true;
    if (nh.nlmsg_type == NLMSG_ERROR)
      return true;

    if (nh.nlmsg_type == RTM_NEWADDR) {
      if (nh.nlmsg_len >= IFA_HDR_END) {
        struct ifaddrmsg ifa{};
        inline_memcpy(&ifa, raw.data() + sizeof(struct nlmsghdr), sizeof(ifa));
        parse_addrconfig_rta(
            ifa.ifa_family,
            raw.subspan(IFA_HDR_END, nh.nlmsg_len - IFA_HDR_END), has_v4,
            has_v6);
      }
    }

    const size_t STEP = align_nl(nh.nlmsg_len, NLMSG_ALIGNTO);
    raw = raw.subspan(STEP < raw.size() ? STEP : raw.size());
  }
  return false;
}

LIBC_INLINE void probe_netlink_addrconfig(bool &allow_v4, bool &allow_v6,
                                          int sock_domain = AF_NETLINK,
                                          uint16_t msg_type = RTM_GETADDR) {
  ErrorOr<int> fd = linux_syscalls::socket(
      sock_domain, SOCK_RAW | SOCK_NONBLOCK | SOCK_CLOEXEC, NETLINK_ROUTE);
  if (!fd.has_value()) {
    allow_v4 = true;
    allow_v6 = true;
    return;
  }

  struct {
    struct nlmsghdr nlh;
    struct ifaddrmsg ifa;
  } req{};
  req.nlh.nlmsg_len = sizeof(req);
  req.nlh.nlmsg_type = msg_type;
  req.nlh.nlmsg_flags = NLM_F_REQUEST;
  if (msg_type == RTM_GETADDR)
    req.nlh.nlmsg_flags |= NLM_F_DUMP;
  req.ifa.ifa_family = AF_UNSPEC;

  allow_v4 = false;
  allow_v6 = false;
  if (linux_syscalls::sendto(*fd, &req, sizeof(req), 0, nullptr, 0)
          .has_value()) {
    alignas(struct nlmsghdr) uint8_t buf[4096];
    while (true) {
      ErrorOr<ssize_t> n =
          linux_syscalls::recvfrom(*fd, buf, sizeof(buf), 0, nullptr, nullptr);
      if (!n.has_value())
        break;
      if (parse_addrconfig_batch({buf, static_cast<size_t>(*n)}, allow_v4,
                                 allow_v6))
        break;
    }
  }

  (void)linux_syscalls::close(*fd);
}

#ifndef LIBC_COPT_PUBLIC_PACKAGING
LIBC_INLINE_VAR LIBC_THREAD_LOCAL bool addrconfig_override_active = false;
LIBC_INLINE_VAR LIBC_THREAD_LOCAL uint8_t addrconfig_override_mask = 0;

LIBC_INLINE void set_addrconfig_override(uint8_t mask) {
  addrconfig_override_active = true;
  addrconfig_override_mask = mask;
}

LIBC_INLINE void clear_addrconfig_override() {
  addrconfig_override_active = false;
  addrconfig_override_mask = 0;
}
#endif // LIBC_COPT_PUBLIC_PACKAGING

LIBC_INLINE void probe_addrconfig(bool &allow_v4, bool &allow_v6) {
#ifndef LIBC_COPT_PUBLIC_PACKAGING
  if (addrconfig_override_active) {
    allow_v4 = (addrconfig_override_mask & 1u) != 0;
    allow_v6 = (addrconfig_override_mask & 2u) != 0;
    return;
  }
#endif // LIBC_COPT_PUBLIC_PACKAGING
  probe_netlink_addrconfig(allow_v4, allow_v6);
}

} // namespace network
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_NETWORK_H
