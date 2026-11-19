//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Streaming reverse /etc/hosts lookup for IPv4 and scoped IPv6 addresses.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_NETDB_HOSTS_REVERSE_H
#define LLVM_LIBC_SRC___SUPPORT_NETDB_HOSTS_REVERSE_H

#include "hdr/errno_macros.h"
#include "hdr/stdint_proxy.h"
#include "hdr/types/size_t.h"
#include "src/__support/CPP/array.h"
#include "src/__support/CPP/scope.h"
#include "src/__support/CPP/span.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/CPP/stringstream.h"
#include "src/__support/File/file.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/netdb/hosts.h"
#include "src/__support/netdb/ip_parse.h"
#include "src/__support/netdb/line_reader.h"
#include "src/string/memory_utils/inline_memcpy.h"

namespace LIBC_NAMESPACE_DECL {
namespace netdb {

struct HostsReverseResult {
  cpp::array<char, DNS_MAX_TEXT_NAME + 1> canon_name{};
  size_t canon_len = 0;
};

LIBC_INLINE constexpr bool all_zero_bytes(cpp::span<const uint8_t> bytes) {
  for (uint8_t b : bytes) {
    if (b != 0)
      return false;
  }
  return true;
}

LIBC_INLINE constexpr bool is_v4_embedded_in6(cpp::span<const uint8_t> bytes) {
  if (!all_zero_bytes(bytes.subspan(0, 10)))
    return false;
  if (bytes[10] == 0xff && bytes[11] == 0xff)
    return true;
  if (bytes[10] != 0 || bytes[11] != 0)
    return false;
  if (!all_zero_bytes(bytes.subspan(12, 3)))
    return true;
  return bytes[15] > 1;
}

LIBC_INLINE constexpr bool scopes_compatible(uint32_t entry_scope,
                                             uint32_t target_scope) {
  if (entry_scope == 0)
    return true;
  if (target_scope == 0)
    return true;
  return entry_scope == target_scope;
}

LIBC_INLINE constexpr bool hosts_ip_matches(const ScopedIpAddr &entry,
                                            bool want_v6,
                                            cpp::span<const uint8_t> want_addr,
                                            uint32_t want_scope) {
  if (entry.is_ipv6 != want_v6)
    return false;

  const size_t BYTES = want_v6 ? 16 : 4;
  for (size_t i = 0; i < BYTES; ++i) {
    if (entry.addr[i] != want_addr[i])
      return false;
  }

  if (!want_v6)
    return true;
  return scopes_compatible(entry.scope_id, want_scope);
}

LIBC_INLINE ErrorOr<ValidHostsLine>
read_reverse_hosts_line(LineReader &reader, bool want_v6,
                        cpp::span<const uint8_t> want_addr, uint32_t want_scope,
                        cpp::StringStream &ip_out, cpp::StringStream &canon_out,
                        cpp::StringStream &alias_out) {
  ErrorOr<HostsLineScan> scan_or =
      scan_hosts_line(reader, {}, ip_out, canon_out, alias_out);
  if (!scan_or.has_value())
    return Error(scan_or.error());

  ValidHostsLine res{};
  if (scan_or->token_count == 0) {
    res.eof = !reader.has_bytes();
    return res;
  }

  if (reader.is_invalid())
    return res;
  if (scan_or->token_count < 2)
    return res;
  if (ip_out.overflow())
    return res;
  if (canon_out.overflow())
    return res;

  const cpp::string_view IP_RAW = ip_out.str();
  const cpp::string_view CANON_RAW = canon_out.str();
  res.ip_tok = IP_RAW.substr(0, IP_RAW.size() - 1);
  res.canon = CANON_RAW.substr(0, CANON_RAW.size() - 1);

  ScopedIpAddr ip{};
  if (!parse_scoped_ip(res.ip_tok, ip))
    return res;
  if (!hosts_ip_matches(ip, want_v6, want_addr, want_scope))
    return res;

  res.alias_count = scan_or->token_count - 2;
  res.valid = true;
  return res;
}

LIBC_INLINE ErrorOr<bool>
lookup_hosts_reverse(bool want_v6, cpp::span<const uint8_t> want_addr,
                     uint32_t want_scope, HostsReverseResult &out,
                     const char *path = nullptr) {
  static constexpr const char *DEFAULT_HOSTS_PATH = "/etc/hosts";
  const char *db_path = path != nullptr ? path : DEFAULT_HOSTS_PATH;
  auto file_or = openfile(db_path, "re");
  if (!file_or.has_value()) {
    if (file_or.error() == ENOENT)
      return false;
    return Error(file_or.error());
  }

  File *file = file_or.value();
  cpp::scope_exit close_file([file]() { file->close(); });
  File::FileLock lock(file);
  LineReader reader(file);

  while (true) {
    constexpr size_t IP_BUF_SIZE = 64;
    char ip_buf[IP_BUF_SIZE];
    char canon_buf[DNS_MAX_TEXT_NAME + 1];
    cpp::StringStream ip_out(ip_buf);
    cpp::StringStream canon_out(canon_buf);
    cpp::StringStream discard_out(cpp::span<char>{});

    ErrorOr<ValidHostsLine> line_or = read_reverse_hosts_line(
        reader, want_v6, want_addr, want_scope, ip_out, canon_out, discard_out);
    if (!line_or.has_value())
      return Error(line_or.error());
    if (line_or->valid) {
      inline_memcpy(out.canon_name.data(), line_or->canon.data(),
                    line_or->canon.size());
      out.canon_name[line_or->canon.size()] = '\0';
      out.canon_len = line_or->canon.size();
      return true;
    }
    if (line_or->eof)
      return false;
  }
}

} // namespace netdb
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_NETDB_HOSTS_REVERSE_H
