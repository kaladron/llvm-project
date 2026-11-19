//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Streaming /etc/hosts forward lookup and RFC 6761 domain classifier.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_NETDB_HOSTS_H
#define LLVM_LIBC_SRC___SUPPORT_NETDB_HOSTS_H

#include "hdr/errno_macros.h"
#include "hdr/stdint_proxy.h"
#include "hdr/types/size_t.h"
#include "src/__support/CPP/array.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/CPP/scope.h"
#include "src/__support/CPP/span.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/CPP/stringstream.h"
#include "src/__support/File/file.h"
#include "src/__support/ctype_utils.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/netdb/ip_parse.h"
#include "src/__support/netdb/line_reader.h"
#include "src/string/memory_utils/inline_memcpy.h"

namespace LIBC_NAMESPACE_DECL {
namespace netdb {

constexpr size_t DNS_MAX_LABEL = 63;
constexpr size_t DNS_MAX_TEXT_NAME = 253;
constexpr size_t HOSTS_MAX_ADDRS = 16;

enum class HostsFamily : uint8_t { IPV4, IPV6, UNSPEC };

enum class SpecialDomainKind : uint8_t { NORMAL, LOCALHOST, INVALID };

struct HostsForwardResult {
  cpp::array<char, DNS_MAX_TEXT_NAME + 1> canon_name{};
  size_t canon_len = 0;
  cpp::array<ScopedIpAddr, HOSTS_MAX_ADDRS> addrs{};
  size_t num_addrs = 0;
};

struct HostsLineScan {
  size_t token_count = 0;
  bool matched = false;
};

#ifndef LIBC_COPT_PUBLIC_PACKAGING
LIBC_INLINE_VAR LIBC_THREAD_LOCAL const char *hosts_path = nullptr;

LIBC_INLINE void set_hosts_path(const char *path) { hosts_path = path; }

LIBC_INLINE const char *get_hosts_path() { return hosts_path; }
#else
LIBC_ALWAYS_INLINE constexpr const char *get_hosts_path() { return nullptr; }
#endif // LIBC_COPT_PUBLIC_PACKAGING

LIBC_INLINE bool ascii_iequals(cpp::string_view a, cpp::string_view b) {
  if (a.size() != b.size())
    return false;

  for (size_t i = 0; i < a.size(); ++i) {
    if (internal::tolower(a[i]) != internal::tolower(b[i]))
      return false;
  }
  return true;
}

LIBC_INLINE constexpr cpp::string_view strip_trailing_dot(cpp::string_view s) {
  if (!s.empty() && s.back() == '.')
    return s.substr(0, s.size() - 1);
  return s;
}

LIBC_INLINE constexpr bool is_valid_dns_label(cpp::string_view label) {
  if (label.empty() || label.size() > DNS_MAX_LABEL)
    return false;

  for (char ch : label) {
    const uint8_t BYTE = static_cast<uint8_t>(ch);
    if (BYTE <= 0x20 || BYTE == 0x7f)
      return false;
  }
  return true;
}

LIBC_INLINE constexpr bool is_valid_domain_name(cpp::string_view trimmed) {
  if (trimmed.size() > DNS_MAX_TEXT_NAME)
    return false;

  for (size_t pos = 0;;) {
    const size_t DOT = trimmed.find_first_of('.', pos);
    const size_t END = DOT == cpp::string_view::npos ? trimmed.size() : DOT;
    if (!is_valid_dns_label(trimmed.substr(pos, END - pos)))
      return false;
    if (DOT == cpp::string_view::npos)
      return true;
    pos = DOT + 1;
  }
}

LIBC_INLINE bool matches_domain_tree(cpp::string_view name,
                                     cpp::string_view tld) {
  if (ascii_iequals(name, tld))
    return true;
  if (name.size() <= tld.size())
    return false;

  const size_t DOT_IDX = name.size() - tld.size() - 1;
  if (name[DOT_IDX] != '.')
    return false;

  return ascii_iequals(name.substr(DOT_IDX + 1), tld);
}

LIBC_INLINE bool has_all_numeric_tld(cpp::string_view trimmed) {
  const size_t DOT = trimmed.find_last_of('.');
  if (DOT == cpp::string_view::npos)
    return false;

  for (size_t i = DOT + 1; i < trimmed.size(); ++i) {
    if (!internal::isdigit(trimmed[i]))
      return false;
  }
  return true;
}

LIBC_INLINE SpecialDomainKind classify_special_domain(cpp::string_view name) {
  const cpp::string_view TRIMMED = strip_trailing_dot(name);
  if (!is_valid_domain_name(TRIMMED))
    return SpecialDomainKind::INVALID;
  if (matches_domain_tree(TRIMMED, "localhost"))
    return SpecialDomainKind::LOCALHOST;
  if (matches_domain_tree(TRIMMED, "invalid"))
    return SpecialDomainKind::INVALID;
  if (has_all_numeric_tld(TRIMMED))
    return SpecialDomainKind::INVALID;
  return SpecialDomainKind::NORMAL;
}

LIBC_INLINE constexpr bool family_allows_ip(HostsFamily fam, bool is_ipv6) {
  if (fam == HostsFamily::UNSPEC)
    return true;
  return (fam == HostsFamily::IPV6) == is_ipv6;
}

LIBC_INLINE void store_canon_if_unset(cpp::string_view canon,
                                      HostsForwardResult &out) {
  if (out.canon_len > 0)
    return;

  inline_memcpy(out.canon_name.data(), canon.data(), canon.size());
  out.canon_name[canon.size()] = '\0';
  out.canon_len = canon.size();
}

struct ValidHostsLine {
  cpp::string_view ip_tok;
  cpp::string_view canon;
  size_t alias_count = 0;
  bool valid = false;
  bool eof = false;
};

LIBC_INLINE ErrorOr<HostsLineScan>
scan_hosts_line(LineReader &reader, cpp::string_view query,
                cpp::StringStream &ip_out, cpp::StringStream &canon_out,
                cpp::StringStream &alias_out) {
  reader.begin_line();
  HostsLineScan scan{};

  while (true) {
    cpp::StringStream *dst = &alias_out;
    cpp::string_view target = query;
    bool ci = true;
    if (scan.token_count == 0) {
      dst = &ip_out;
      target = {};
      ci = false;
    } else if (scan.token_count == 1) {
      dst = &canon_out;
    }

    ErrorOr<cpp::optional<bool>> tok_or = reader.read_token(*dst, target, ci);
    if (!tok_or.has_value())
      return Error(tok_or.error());
    if (!tok_or->has_value())
      return scan;

    if (**tok_or)
      scan.matched = true;
    ++scan.token_count;
  }
}

LIBC_INLINE ErrorOr<ValidHostsLine>
read_valid_hosts_line(LineReader &reader, cpp::string_view query,
                      cpp::StringStream &ip_out, cpp::StringStream &canon_out,
                      cpp::StringStream &alias_out) {
  ErrorOr<HostsLineScan> scan_or =
      scan_hosts_line(reader, query, ip_out, canon_out, alias_out);
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
  if (!scan_or->matched)
    return res;
  if (ip_out.overflow())
    return res;
  if (canon_out.overflow())
    return res;

  const cpp::string_view IP_RAW = ip_out.str();
  const cpp::string_view CANON_RAW = canon_out.str();
  res.ip_tok = IP_RAW.substr(0, IP_RAW.size() - 1);
  res.canon = CANON_RAW.substr(0, CANON_RAW.size() - 1);
  res.alias_count = scan_or->token_count - 2;
  res.valid = true;
  return res;
}

LIBC_INLINE ErrorOr<bool> parse_hosts_forward_line(LineReader &reader,
                                                   cpp::string_view query,
                                                   HostsFamily fam,
                                                   HostsForwardResult &out) {
  constexpr size_t IP_BUF_SIZE = 64;
  char ip_buf[IP_BUF_SIZE];
  char canon_buf[DNS_MAX_TEXT_NAME + 1];
  cpp::StringStream ip_out(ip_buf);
  cpp::StringStream canon_out(canon_buf);
  cpp::StringStream discard_out(cpp::span<char>{});

  ErrorOr<ValidHostsLine> line_or =
      read_valid_hosts_line(reader, query, ip_out, canon_out, discard_out);
  if (!line_or.has_value())
    return Error(line_or.error());
  if (!line_or->valid)
    return line_or->eof;

  ScopedIpAddr ip{};
  if (!parse_scoped_ip(line_or->ip_tok, ip))
    return false;
  if (!family_allows_ip(fam, ip.is_ipv6))
    return false;

  store_canon_if_unset(line_or->canon, out);
  if (out.num_addrs < HOSTS_MAX_ADDRS)
    out.addrs[out.num_addrs++] = ip;
  return false;
}

LIBC_INLINE ErrorOr<bool> lookup_hosts_forward(cpp::string_view query,
                                               HostsFamily fam,
                                               HostsForwardResult &out,
                                               const char *path = nullptr) {
  static constexpr const char *DEFAULT_HOSTS_PATH = "/etc/hosts";
  const SpecialDomainKind KIND = classify_special_domain(query);
  if (KIND == SpecialDomainKind::INVALID)
    return false;

  const cpp::string_view TRIMMED = strip_trailing_dot(query);
  const char *db_path = path != nullptr ? path : DEFAULT_HOSTS_PATH;
  auto file_or = openfile(db_path, "re");
  if (!file_or.has_value()) {
    if (file_or.error() != ENOENT)
      return Error(file_or.error());
  } else {
    File *file = file_or.value();
    cpp::scope_exit close_file([file]() { file->close(); });
    File::FileLock lock(file);
    LineReader reader(file);

    while (true) {
      ErrorOr<bool> eof_or =
          parse_hosts_forward_line(reader, TRIMMED, fam, out);
      if (!eof_or.has_value())
        return Error(eof_or.error());
      if (*eof_or)
        break;
    }
  }

  if (out.num_addrs == 0 && KIND == SpecialDomainKind::LOCALHOST) {
    store_canon_if_unset(TRIMMED, out);
    if (family_allows_ip(fam, false)) {
      ScopedIpAddr v4{};
      v4.addr[0] = 127;
      v4.addr[3] = 1;
      out.addrs[out.num_addrs++] = v4;
    }
    if (family_allows_ip(fam, true)) {
      ScopedIpAddr v6{};
      v6.is_ipv6 = true;
      v6.addr[15] = 1;
      out.addrs[out.num_addrs++] = v6;
    }
  }

  return out.num_addrs > 0;
}

} // namespace netdb
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_NETDB_HOSTS_H
