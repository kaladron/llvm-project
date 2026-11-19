//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Internal Linux network interface, Netlink, and non-blocking UDP helpers.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_NETWORK_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_NETWORK_H

#include "hdr/errno_macros.h"
#include "hdr/net_if_macros.h"
#include "hdr/poll_macros.h"
#include "hdr/stdint_proxy.h"
#include "hdr/sys_ioctl_macros.h"
#include "hdr/sys_socket_macros.h"
#include "hdr/types/size_t.h"
#include "hdr/types/socklen_t.h"
#include "hdr/types/ssize_t.h"
#include "hdr/types/struct_ifreq.h"
#include "hdr/types/struct_pollfd.h"
#include "hdr/types/struct_sockaddr.h"
#include "hdr/types/struct_sockaddr_in.h"
#include "hdr/types/struct_sockaddr_in6.h"
#include "src/__support/CPP/span.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/close.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/connect.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/getrandom.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/getsockopt.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/ioctl.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/poll.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/recvfrom.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/sendto.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/socket.h"
#include "src/__support/endian_internal.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/string/memory_utils/inline_memcpy.h"

#include <linux/netlink.h>
#include <linux/rtnetlink.h>

namespace LIBC_NAMESPACE_DECL {
namespace network {

enum class UdpRecvKind : uint8_t { PACKET, TRUNCATED, TIMEOUT, HARD_ERROR };

struct UdpRecvResult {
  UdpRecvKind kind;
  size_t len;
};

LIBC_INLINE void close_fd(int fd) { (void)linux_syscalls::close(fd); }

LIBC_INLINE uint16_t random_u16(uint16_t seed) {
  (void)linux_syscalls::getrandom(&seed, sizeof(seed), 0);
  return seed;
}

LIBC_INLINE bool poll_fd_ready(int fd, short events, uint32_t timeout_ms) {
  struct pollfd pfd{fd, events, 0};
  struct pollfd *ptr = fd < -1 ? nullptr : &pfd;
  ErrorOr<int> rc = linux_syscalls::poll(ptr, 1, static_cast<int>(timeout_ms));
  if (!rc.has_value())
    return false;
  return *rc > 0;
}

LIBC_INLINE ErrorOr<int> connect_endpoint(int fd, bool is_ipv6,
                                          cpp::span<const uint8_t> addr,
                                          uint32_t scope_id, uint16_t port) {
  const uint16_t NET_PORT = Endian::to_big_endian(port);
  if (is_ipv6) {
    struct sockaddr_in6 s6{};
    s6.sin6_family = AF_INET6;
    s6.sin6_port = NET_PORT;
    s6.sin6_scope_id = scope_id;
    inline_memcpy(&s6.sin6_addr, addr.data(), sizeof(s6.sin6_addr));
    return linux_syscalls::connect(
        fd, reinterpret_cast<const struct sockaddr *>(&s6), sizeof(s6));
  }

  struct sockaddr_in s4{};
  s4.sin_family = AF_INET;
  s4.sin_port = NET_PORT;
  inline_memcpy(&s4.sin_addr, addr.data(), sizeof(s4.sin_addr));
  return linux_syscalls::connect(
      fd, reinterpret_cast<const struct sockaddr *>(&s4), sizeof(s4));
}

LIBC_INLINE int finish_connect(int fd, const ErrorOr<int> &cr,
                               uint32_t timeout_ms) {
  if (cr.has_value())
    return fd;
  if (cr.error() != EINPROGRESS) {
    close_fd(fd);
    return -1;
  }

  int so_err = ETIMEDOUT;
  socklen_t err_len = sizeof(so_err);
  if (poll_fd_ready(fd, POLLOUT, timeout_ms)) {
    (void)linux_syscalls::getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_err,
                                     &err_len);
  }
  if (so_err != 0) {
    close_fd(fd);
    return -1;
  }
  return fd;
}

LIBC_INLINE int open_sock(bool is_ipv6, int sock_type,
                          cpp::span<const uint8_t> addr, uint32_t scope_id,
                          uint16_t port, uint32_t timeout_ms) {
  const int DOMAIN = is_ipv6 ? AF_INET6 : AF_INET;
  ErrorOr<int> fd = linux_syscalls::socket(
      DOMAIN, sock_type | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (!fd.has_value())
    return -1;

  return finish_connect(
      *fd, connect_endpoint(*fd, is_ipv6, addr, scope_id, port), timeout_ms);
}

LIBC_INLINE int open_udp(bool is_ipv6, cpp::span<const uint8_t> addr,
                         uint32_t scope_id, uint16_t port,
                         int extra_flags = 0) {
  return open_sock(is_ipv6, SOCK_DGRAM | extra_flags, addr, scope_id, port, 0);
}

LIBC_INLINE int open_tcp(bool is_ipv6, cpp::span<const uint8_t> addr,
                         uint32_t scope_id, uint16_t port,
                         uint32_t timeout_ms) {
  return open_sock(is_ipv6, SOCK_STREAM, addr, scope_id, port, timeout_ms);
}

LIBC_INLINE ssize_t send_bytes(int fd, cpp::span<const uint8_t> pkt,
                               int flags) {
  ErrorOr<ssize_t> rc =
      linux_syscalls::sendto(fd, pkt.data(), pkt.size(), flags, nullptr, 0);
  if (!rc.has_value())
    return -1;
  return *rc;
}

LIBC_INLINE ssize_t udp_send(int fd, cpp::span<const uint8_t> pkt) {
  return send_bytes(fd, pkt, MSG_NOSIGNAL);
}

LIBC_INLINE ssize_t tcp_send(int fd, cpp::span<const uint8_t> pkt,
                             uint32_t timeout_ms) {
  if (!poll_fd_ready(fd, POLLOUT, timeout_ms))
    return -1;
  return send_bytes(fd, pkt, MSG_DONTWAIT | MSG_NOSIGNAL);
}

LIBC_INLINE ssize_t tcp_recv(int fd, cpp::span<uint8_t> out,
                             uint32_t timeout_ms) {
  if (!poll_fd_ready(fd, POLLIN, timeout_ms))
    return -1;

  ErrorOr<ssize_t> n = linux_syscalls::recvfrom(fd, out.data(), out.size(),
                                                MSG_DONTWAIT, nullptr, nullptr);
  if (!n.has_value())
    return -1;
  return *n;
}

LIBC_INLINE UdpRecvResult udp_poll_recv(int fd, cpp::span<uint8_t> out,
                                        uint32_t timeout_ms) {
  if (!poll_fd_ready(fd, POLLIN, timeout_ms))
    return {UdpRecvKind::TIMEOUT, 0};

  ErrorOr<ssize_t> n = linux_syscalls::recvfrom(
      fd, out.data(), out.size(), MSG_DONTWAIT | MSG_TRUNC, nullptr, nullptr);
  if (!n.has_value())
    return {UdpRecvKind::HARD_ERROR, 0};

  const size_t LEN = static_cast<size_t>(*n);
  if (LEN > out.size())
    return {UdpRecvKind::TRUNCATED, LEN};

  return {UdpRecvKind::PACKET, LEN};
}

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
  close_fd(*fd);
  if (!rc.has_value())
    return 0;

  return static_cast<uint32_t>(ifr.ifr_ifindex);
}

LIBC_INLINE bool if_indextoname(uint32_t ifindex, cpp::span<char> out,
                                int sock_domain = AF_UNIX) {
  ErrorOr<int> fd =
      linux_syscalls::socket(sock_domain, SOCK_DGRAM | SOCK_CLOEXEC, 0);
  if (!fd.has_value())
    return false;

  struct ifreq ifr{};
  ifr.ifr_ifindex = static_cast<int>(ifindex);
  ErrorOr<int> rc = linux_syscalls::ioctl(*fd, SIOCGIFNAME, &ifr);
  close_fd(*fd);
  if (!rc.has_value())
    return false;

  inline_memcpy(out.data(), ifr.ifr_name, IF_NAMESIZE);
  out[IF_NAMESIZE - 1] = '\0';
  return true;
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

  close_fd(*fd);
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
