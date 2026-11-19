//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Streaming /etc/resolv.conf parser with loopback fallback.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_NETDB_RESOLV_CONF_H
#define LLVM_LIBC_SRC___SUPPORT_NETDB_RESOLV_CONF_H

#include "hdr/errno_macros.h"
#include "hdr/stdint_proxy.h"
#include "hdr/types/size_t.h"
#include "src/__support/CPP/algorithm.h"
#include "src/__support/CPP/array.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/CPP/scope.h"
#include "src/__support/CPP/span.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/CPP/stringstream.h"
#include "src/__support/File/file.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/netdb/ip_parse.h"
#include "src/__support/netdb/line_reader.h"

namespace LIBC_NAMESPACE_DECL {
namespace netdb {

constexpr size_t RESOLV_MAX_NS = 3;

struct ResolvConfig {
  cpp::array<ScopedIpAddr, RESOLV_MAX_NS> nameservers{};
  size_t num_nameservers = 0;
  uint8_t timeout = 5;
  uint8_t attempts = 2;
  bool rotate = false;
};

#ifndef LIBC_COPT_PUBLIC_PACKAGING
LIBC_INLINE_VAR LIBC_THREAD_LOCAL const char *resolv_conf_path = nullptr;

LIBC_INLINE void set_resolv_conf_path(const char *path) {
  resolv_conf_path = path;
}

LIBC_INLINE const char *get_resolv_conf_path() { return resolv_conf_path; }
#else
LIBC_ALWAYS_INLINE constexpr const char *get_resolv_conf_path() {
  return nullptr;
}
#endif // LIBC_COPT_PUBLIC_PACKAGING

LIBC_INLINE void parse_clamped_opt(cpp::string_view opt,
                                   cpp::string_view prefix, uint8_t lo,
                                   uint8_t hi, uint8_t &dst) {
  if (opt.size() <= prefix.size())
    return;
  if (opt.substr(0, prefix.size()) != prefix)
    return;

  const cpp::optional<uint32_t> VAL =
      parse_unsigned<uint32_t>(opt.substr(prefix.size()));
  if (!VAL.has_value())
    return;

  dst = static_cast<uint8_t>(cpp::clamp<uint32_t>(*VAL, lo, hi));
}

LIBC_INLINE void apply_resolv_option(cpp::string_view opt, ResolvConfig &cfg) {
  if (opt == "rotate") {
    cfg.rotate = true;
    return;
  }

  parse_clamped_opt(opt, "timeout:", 1, 30, cfg.timeout);
  parse_clamped_opt(opt, "attempts:", 1, 5, cfg.attempts);
}

LIBC_INLINE ErrorOr<size_t> drain_remaining_tokens(LineReader &reader) {
  size_t count = 0;
  cpp::StringStream discard_out(cpp::span<char>{});
  while (true) {
    ErrorOr<cpp::optional<bool>> tok_or = reader.read_token(discard_out, {});
    if (!tok_or.has_value())
      return Error(tok_or.error());
    if (!tok_or->has_value())
      return count;
    ++count;
  }
}

LIBC_INLINE ErrorOr<void> parse_nameserver_args(LineReader &reader,
                                                ResolvConfig &cfg) {
  constexpr size_t IP_BUF_SIZE = 64;
  char ip_buf[IP_BUF_SIZE];
  cpp::StringStream ip_out(ip_buf);

  ErrorOr<cpp::optional<bool>> ip_tok_or = reader.read_token(ip_out, {});
  if (!ip_tok_or.has_value())
    return Error(ip_tok_or.error());
  if (!ip_tok_or->has_value())
    return {};

  ErrorOr<size_t> extra_or = drain_remaining_tokens(reader);
  if (!extra_or.has_value())
    return Error(extra_or.error());

  if (reader.is_invalid())
    return {};
  if (*extra_or != 0)
    return {};
  if (ip_out.overflow())
    return {};
  if (cfg.num_nameservers >= RESOLV_MAX_NS)
    return {};

  const cpp::string_view RAW = ip_out.str();
  ScopedIpAddr ns{};
  if (parse_scoped_ip(RAW.substr(0, RAW.size() - 1), ns))
    cfg.nameservers[cfg.num_nameservers++] = ns;
  return {};
}

LIBC_INLINE ErrorOr<void> parse_options_args(LineReader &reader,
                                             ResolvConfig &cfg) {
  ResolvConfig line_cfg = cfg;
  while (true) {
    constexpr size_t OPT_BUF_SIZE = 32;
    char opt_buf[OPT_BUF_SIZE];
    cpp::StringStream opt_out(opt_buf);
    ErrorOr<cpp::optional<bool>> tok_or = reader.read_token(opt_out, {});
    if (!tok_or.has_value())
      return Error(tok_or.error());
    if (!tok_or->has_value())
      break;
    if (!opt_out.overflow()) {
      const cpp::string_view RAW = opt_out.str();
      apply_resolv_option(RAW.substr(0, RAW.size() - 1), line_cfg);
    }
  }

  if (!reader.is_invalid())
    cfg = line_cfg;
  return {};
}

LIBC_INLINE ErrorOr<bool> parse_resolv_line(LineReader &reader,
                                            ResolvConfig &cfg) {
  reader.begin_line();
  constexpr size_t KW_BUF_SIZE = 24;
  char kw_buf[KW_BUF_SIZE];
  cpp::StringStream kw_out(kw_buf);

  ErrorOr<cpp::optional<bool>> kw_or = reader.read_token(kw_out, {});
  if (!kw_or.has_value())
    return Error(kw_or.error());
  if (!kw_or->has_value())
    return !reader.has_bytes();

  cpp::string_view kw{};
  if (!kw_out.overflow()) {
    const cpp::string_view RAW = kw_out.str();
    kw = RAW.substr(0, RAW.size() - 1);
  }

  if (kw == "nameserver") {
    ErrorOr<void> ns_res = parse_nameserver_args(reader, cfg);
    if (!ns_res.has_value())
      return Error(ns_res.error());
    return false;
  }

  if (kw == "options") {
    ErrorOr<void> opt_res = parse_options_args(reader, cfg);
    if (!opt_res.has_value())
      return Error(opt_res.error());
    return false;
  }

  ErrorOr<size_t> drain_or = drain_remaining_tokens(reader);
  if (!drain_or.has_value())
    return Error(drain_or.error());
  return false;
}

LIBC_INLINE ErrorOr<void> load_resolv_conf(ResolvConfig &cfg,
                                           const char *path = nullptr) {
  static constexpr const char *DEFAULT_RESOLV_PATH = "/etc/resolv.conf";
  const char *db_path = path != nullptr ? path : DEFAULT_RESOLV_PATH;
  auto file_or = openfile(db_path, "re");
  if (!file_or.has_value()) {
    if (file_or.error() != ENOENT)
      return Error(file_or.error());
  } else {
    File *file = file_or.value();
    cpp::scope_exit close_file([file]() { file->close(); });
    File::FileLock lock(file);
    LineReader reader(file, ';');

    while (true) {
      ErrorOr<bool> eof_or = parse_resolv_line(reader, cfg);
      if (!eof_or.has_value())
        return Error(eof_or.error());
      if (*eof_or)
        break;
    }
  }

  if (cfg.num_nameservers == 0) {
    cfg.nameservers[0].addr[0] = 127;
    cfg.nameservers[0].addr[3] = 1;
    cfg.num_nameservers = 1;
  }
  return {};
}

} // namespace netdb
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_NETDB_RESOLV_CONF_H
