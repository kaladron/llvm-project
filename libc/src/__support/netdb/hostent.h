//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Caller-buffer struct hostent packer and IPv4/IPv6 host lookup driver.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_NETDB_HOSTENT_H
#define LLVM_LIBC_SRC___SUPPORT_NETDB_HOSTENT_H

#include "hdr/errno_macros.h"
#include "hdr/netdb_macros.h"
#include "hdr/stdint_proxy.h"
#include "hdr/sys_socket_macros.h"
#include "hdr/types/size_t.h"
#include "hdr/types/struct_hostent.h"
#include "hdr/types/struct_in6_addr.h"
#include "hdr/types/struct_in_addr.h"
#include "src/__support/CPP/array.h"
#include "src/__support/CPP/scope.h"
#include "src/__support/CPP/span.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/CPP/stringstream.h"
#include "src/__support/File/file.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/memory_size.h"
#include "src/__support/net/address.h"
#include "src/__support/netdb/dns_client.h"
#include "src/__support/netdb/hosts.h"
#include "src/__support/netdb/ip_parse.h"
#include "src/__support/netdb/line_reader.h"
#include "src/__support/netdb/resolv_conf.h"
#include "src/string/memory_utils/inline_memcpy.h"

namespace LIBC_NAMESPACE_DECL {
namespace netdb {

static_assert(alignof(char *) >= alignof(struct in_addr) &&
              alignof(char *) >= alignof(struct in6_addr));

struct HostentScanState {
  cpp::array<cpp::array<uint8_t, 16>, HOSTS_MAX_ADDRS> addrs{};
  size_t num_addrs = 0;
  size_t alias_count = 0;
  size_t text_len = 0;
  bool text_overflow = false;
};

LIBC_INLINE void set_herr_out(int *h_errnop, int val) {
  if (h_errnop != nullptr)
    *h_errnop = val;
}

LIBC_INLINE constexpr size_t host_family_len(int af) {
  if (af == AF_INET)
    return sizeof(struct in_addr);
  if (af == AF_INET6)
    return sizeof(struct in6_addr);
  return 0;
}

LIBC_INLINE int
pack_hostent_layout(cpp::span<char> buffer, size_t text_len, size_t alias_count,
                    cpp::span<const cpp::array<uint8_t, 16>> addrs, int af,
                    size_t addr_len, struct hostent *ret,
                    struct hostent **result) {
  cpp::span<char> tail = buffer.subspan(text_len);
  const uintptr_t TAIL_ADDR = reinterpret_cast<uintptr_t>(tail.data());
  const size_t PADDING =
      internal::SafeMemSize::offset_to(TAIL_ADDR, alignof(char *));
  if (tail.size() < PADDING)
    return ERANGE;

  tail = tail.subspan(PADDING);
  const size_t PTR_COUNT = alias_count + addrs.size() + 2;
  if (tail.size() / sizeof(char *) < PTR_COUNT)
    return ERANGE;

  char **ptrs = reinterpret_cast<char **>(tail.data());
  tail = tail.subspan(PTR_COUNT * sizeof(char *));
  const size_t ADDR_BYTES = addrs.size() * addr_len;
  if (tail.size() < ADDR_BYTES)
    return ERANGE;

  char **aliases = ptrs;
  char **addr_list = ptrs + alias_count + 1;
  char *cursor = buffer.data();

  for (size_t i = 0; i < alias_count; ++i) {
    aliases[i] = cursor;
    while (*cursor != '\0')
      ++cursor;
    ++cursor;
  }
  aliases[alias_count] = nullptr;
  ret->h_name = cursor;

  char *addr_dst = tail.data();
  for (size_t i = 0; i < addrs.size(); ++i) {
    addr_list[i] = addr_dst;
    inline_memcpy(addr_dst, addrs[i].data(), addr_len);
    addr_dst += addr_len;
  }
  addr_list[addrs.size()] = nullptr;

  ret->h_aliases = aliases;
  ret->h_addrtype = af;
  ret->h_length = static_cast<int>(addr_len);
  ret->h_addr_list = addr_list;
  *result = ret;
  return 0;
}

LIBC_INLINE int
pack_named_hostent(cpp::string_view canon_name,
                   cpp::span<const cpp::array<uint8_t, 16>> addrs, int af,
                   size_t addr_len, cpp::span<char> buffer, struct hostent *ret,
                   struct hostent **result, int *h_errnop) {
  cpp::StringStream out(buffer);
  out << canon_name << cpp::StringStream::ENDS;
  if (out.overflow()) {
    set_herr_out(h_errnop, NETDB_INTERNAL);
    return ERANGE;
  }

  const int RC = pack_hostent_layout(buffer, out.str().size(), 0, addrs, af,
                                     addr_len, ret, result);
  if (RC != 0)
    set_herr_out(h_errnop, NETDB_INTERNAL);
  return RC;
}

LIBC_INLINE bool try_parse_host_literal(cpp::string_view name, bool is_ipv6,
                                        cpp::array<uint8_t, 16> &out) {
  if (is_ipv6) {
    struct in6_addr v6{};
    if (!net::str_to_ipv6(name, v6))
      return false;
    inline_memcpy(out.data(), &v6, sizeof(v6));
    return true;
  }

  struct in_addr v4{};
  if (!net::str_to_ipv4(name, v4))
    return false;
  inline_memcpy(out.data(), &v4, sizeof(v4));
  return true;
}

LIBC_INLINE ErrorOr<bool> parse_hostent_hosts_line(LineReader &reader,
                                                   cpp::string_view query,
                                                   bool is_ipv6,
                                                   cpp::span<char> buffer,
                                                   HostentScanState &state) {
  constexpr size_t IP_BUF_SIZE = 64;
  char ip_buf[IP_BUF_SIZE];
  char canon_buf[DNS_MAX_TEXT_NAME + 1];
  cpp::StringStream ip_out(ip_buf);
  cpp::StringStream canon_out(canon_buf);
  cpp::StringStream candidate_out(buffer);
  cpp::StringStream discard_out(cpp::span<char>{});

  cpp::StringStream *alias_dst = &discard_out;
  if (state.num_addrs == 0)
    alias_dst = &candidate_out;

  ErrorOr<ValidHostsLine> line_or =
      read_valid_hosts_line(reader, query, ip_out, canon_out, *alias_dst);
  if (!line_or.has_value())
    return Error(line_or.error());
  if (!line_or->valid)
    return line_or->eof;

  ScopedIpAddr ip{};
  if (!parse_scoped_ip(line_or->ip_tok, ip))
    return false;
  if (ip.is_ipv6 != is_ipv6)
    return false;

  if (state.num_addrs == 0) {
    candidate_out << line_or->canon << cpp::StringStream::ENDS;
    state.alias_count = line_or->alias_count;
    state.text_len = candidate_out.str().size();
    state.text_overflow = candidate_out.overflow();
  }

  if (state.num_addrs < HOSTS_MAX_ADDRS)
    state.addrs[state.num_addrs++] = ip.addr;
  return false;
}

LIBC_INLINE ErrorOr<void> scan_hosts_file(cpp::string_view trimmed,
                                          bool is_ipv6, cpp::span<char> buffer,
                                          HostentScanState &state,
                                          const char *path) {
  static constexpr const char *DEFAULT_HOSTS_PATH = "/etc/hosts";
  const char *db_path = path != nullptr ? path : DEFAULT_HOSTS_PATH;
  auto file_or = openfile(db_path, "re");
  if (!file_or.has_value()) {
    if (file_or.error() == ENOENT)
      return {};
    return Error(file_or.error());
  }

  File *file = file_or.value();
  cpp::scope_exit close_file([file]() { file->close(); });
  File::FileLock lock(file);
  LineReader reader(file);

  while (true) {
    ErrorOr<bool> eof_or =
        parse_hostent_hosts_line(reader, trimmed, is_ipv6, buffer, state);
    if (!eof_or.has_value())
      return Error(eof_or.error());
    if (*eof_or)
      return {};
  }
}

LIBC_INLINE constexpr int dns_status_to_h_errno(DnsStatus st) {
  if (st == DnsStatus::NODATA)
    return NO_DATA;
  if (st == DnsStatus::RETRY)
    return TRY_AGAIN;
  if (st == DnsStatus::FAIL)
    return NO_RECOVERY;
  return HOST_NOT_FOUND;
}

LIBC_INLINE int lookup_host_byname(cpp::string_view name, int af,
                                   struct hostent *ret, cpp::span<char> buffer,
                                   struct hostent **result, int *h_errnop,
                                   const char *path = nullptr) {
  *result = nullptr;

  const size_t ADDR_LEN = host_family_len(af);
  if (ADDR_LEN == 0) {
    set_herr_out(h_errnop, NO_RECOVERY);
    return EAFNOSUPPORT;
  }

  const bool IS_IPV6 = af == AF_INET6;
  cpp::array<uint8_t, 16> literal_addr{};
  if (try_parse_host_literal(name, IS_IPV6, literal_addr))
    return pack_named_hostent(name, {&literal_addr, 1}, af, ADDR_LEN, buffer,
                              ret, result, h_errnop);

  const SpecialDomainKind KIND = classify_special_domain(name);
  if (KIND == SpecialDomainKind::INVALID) {
    set_herr_out(h_errnop, HOST_NOT_FOUND);
    return 0;
  }

  const cpp::string_view TRIMMED = strip_trailing_dot(name);
  HostentScanState state{};
  ErrorOr<void> scan_res =
      scan_hosts_file(TRIMMED, IS_IPV6, buffer, state, path);
  if (!scan_res.has_value()) {
    set_herr_out(h_errnop, NO_RECOVERY);
    return scan_res.error();
  }

  if (state.num_addrs > 0) {
    if (state.text_overflow) {
      set_herr_out(h_errnop, NETDB_INTERNAL);
      return ERANGE;
    }

    const int RC = pack_hostent_layout(
        buffer, state.text_len, state.alias_count,
        {state.addrs.data(), state.num_addrs}, af, ADDR_LEN, ret, result);
    if (RC != 0)
      set_herr_out(h_errnop, NETDB_INTERNAL);
    return RC;
  }

  if (KIND == SpecialDomainKind::LOCALHOST) {
    cpp::array<uint8_t, 16> lo{};
    if (IS_IPV6) {
      lo[15] = 1;
    } else {
      lo[0] = 127;
      lo[3] = 1;
    }
    return pack_named_hostent(TRIMMED, {&lo, 1}, af, ADDR_LEN, buffer, ret,
                              result, h_errnop);
  }

  ResolvConfig cfg{};
  ErrorOr<void> rcfg = load_resolv_conf(cfg, get_resolv_conf_path());
  if (!rcfg.has_value()) {
    set_herr_out(h_errnop, NO_RECOVERY);
    return rcfg.error();
  }

  uint16_t qtype = DNS_TYPE_A;
  if (IS_IPV6)
    qtype = DNS_TYPE_AAAA;

  DnsParsedResponse dns{};
  const DnsStatus ST = dispatch_dns_name(cfg, name, qtype, dns);
  if (ST == DnsStatus::OK) {
    cpp::array<char, DNS_MAX_TEXT_NAME + 1> canon_buf{};
    const size_t CANON_LEN =
        wire_to_presentation({dns.canon_wire.data(), dns.clen}, canon_buf);
    return pack_named_hostent({canon_buf.data(), CANON_LEN},
                              {dns.addrs.data(), dns.num_addrs}, af, ADDR_LEN,
                              buffer, ret, result, h_errnop);
  }

  set_herr_out(h_errnop, dns_status_to_h_errno(ST));
  return 0;
}

} // namespace netdb
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_NETDB_HOSTENT_H
