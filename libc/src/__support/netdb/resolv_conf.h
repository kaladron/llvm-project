//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Streaming /etc/resolv.conf parser with search domains and loopback fallback.
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
#include "src/__support/netdb/dns_wire.h"
#include "src/__support/netdb/hosts.h"
#include "src/__support/netdb/ip_parse.h"
#include "src/__support/netdb/line_reader.h"
#include "src/string/memory_utils/inline_memcpy.h"

namespace LIBC_NAMESPACE_DECL {
namespace netdb {

constexpr size_t RESOLV_MAX_NS = 3;
constexpr size_t RESOLV_MAX_SEARCH = 6;
constexpr size_t RESOLV_SEARCH_BUF = 256;

struct ResolvSlice {
  uint16_t offset = 0;
  uint16_t len = 0;
};

struct ResolvConfig {
  cpp::array<ScopedIpAddr, RESOLV_MAX_NS> nameservers{};
  size_t num_nameservers = 0;
  cpp::array<uint8_t, RESOLV_SEARCH_BUF> search_buf{};
  cpp::array<ResolvSlice, RESOLV_MAX_SEARCH> search{};
  size_t num_search = 0;
  size_t search_used = 0;
  uint8_t ndots = 1;
  uint8_t timeout = 5;
  uint8_t attempts = 2;
  bool rotate = false;
  bool use_vc = false;
};

enum class ResolvDirective : uint8_t {
  UNKNOWN,
  NAMESERVER,
  OPTIONS,
  DOMAIN_ONE,
  SEARCH_LIST,
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
  if (opt == "use-vc" || opt == "tcp") {
    cfg.use_vc = true;
    return;
  }

  parse_clamped_opt(opt, "ndots:", 0, 15, cfg.ndots);
  parse_clamped_opt(opt, "timeout:", 1, 30, cfg.timeout);
  parse_clamped_opt(opt, "attempts:", 1, 5, cfg.attempts);
}

LIBC_INLINE void append_search_domain(cpp::string_view dom, ResolvConfig &cfg) {
  if (cfg.num_search >= RESOLV_MAX_SEARCH)
    return;

  cpp::array<uint8_t, DNS_MAX_WIRE_NAME> encoded{};
  const cpp::optional<EncodedQname> ENC = encode_qname(dom, encoded);
  if (!ENC.has_value())
    return;
  if (RESOLV_SEARCH_BUF - cfg.search_used < ENC->wire_len)
    return;

  cfg.search[cfg.num_search++] = {static_cast<uint16_t>(cfg.search_used),
                                  static_cast<uint16_t>(ENC->wire_len)};
  inline_memcpy(cfg.search_buf.data() + cfg.search_used, encoded.data(),
                ENC->wire_len);
  cfg.search_used += ENC->wire_len;
}

LIBC_INLINE constexpr ResolvDirective
classify_resolv_directive(cpp::string_view kw) {
  if (kw == "nameserver")
    return ResolvDirective::NAMESERVER;
  if (kw == "options")
    return ResolvDirective::OPTIONS;
  if (kw == "domain")
    return ResolvDirective::DOMAIN_ONE;
  if (kw == "search")
    return ResolvDirective::SEARCH_LIST;
  return ResolvDirective::UNKNOWN;
}

LIBC_INLINE void consume_resolv_arg(ResolvDirective dir, size_t token_index,
                                    cpp::string_view tok, bool tok_overflow,
                                    cpp::array<char, 64> &ns_buf,
                                    size_t &ns_len, bool &ns_overflow,
                                    ResolvConfig &line_cfg) {
  if (dir == ResolvDirective::NAMESERVER) {
    if (token_index == 1) {
      if (tok_overflow || tok.size() >= ns_buf.size()) {
        ns_overflow = true;
      } else {
        inline_memcpy(ns_buf.data(), tok.data(), tok.size());
        ns_len = tok.size();
      }
    }
    return;
  }

  if (tok_overflow)
    return;

  if (dir == ResolvDirective::OPTIONS) {
    apply_resolv_option(tok, line_cfg);
  } else if (dir == ResolvDirective::DOMAIN_ONE) {
    if (token_index == 1)
      append_search_domain(tok, line_cfg);
  } else if (dir == ResolvDirective::SEARCH_LIST) {
    append_search_domain(tok, line_cfg);
  }
}

LIBC_INLINE ErrorOr<bool> parse_resolv_line(LineReader &reader,
                                            ResolvConfig &cfg) {
  reader.begin_line();
  ResolvConfig line_cfg = cfg;
  ResolvDirective dir = ResolvDirective::UNKNOWN;
  size_t token_index = 0;
  cpp::array<char, 64> ns_buf{};
  size_t ns_len = 0;
  bool ns_overflow = false;

  while (true) {
    char tok_buf[DNS_MAX_TEXT_NAME + 2];
    cpp::StringStream tok_out(tok_buf);
    ErrorOr<cpp::optional<bool>> tok_or = reader.read_token(tok_out, {});
    if (!tok_or.has_value())
      return Error(tok_or.error());
    if (!tok_or->has_value())
      break;

    const bool OVERFLOWED = tok_out.overflow();
    cpp::string_view tok{};
    if (!OVERFLOWED) {
      const cpp::string_view RAW = tok_out.str();
      tok = RAW.substr(0, RAW.size() - 1);
    }

    if (token_index == 0) {
      dir = classify_resolv_directive(tok);
      if (dir == ResolvDirective::DOMAIN_ONE ||
          dir == ResolvDirective::SEARCH_LIST) {
        line_cfg.num_search = 0;
        line_cfg.search_used = 0;
      }
    } else {
      consume_resolv_arg(dir, token_index, tok, OVERFLOWED, ns_buf, ns_len,
                         ns_overflow, line_cfg);
    }
    ++token_index;
  }

  if (token_index == 0)
    return !reader.has_bytes();
  if (reader.is_invalid())
    return false;

  if (dir == ResolvDirective::NAMESERVER) {
    if (token_index == 2 && !ns_overflow &&
        line_cfg.num_nameservers < RESOLV_MAX_NS) {
      ScopedIpAddr ns{};
      if (parse_scoped_ip({ns_buf.data(), ns_len}, ns))
        line_cfg.nameservers[line_cfg.num_nameservers++] = ns;
    }
  }

  cfg = line_cfg;
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
