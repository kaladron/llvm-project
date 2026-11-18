//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of shared /etc/networks lookup helpers.
///
//===----------------------------------------------------------------------===//

#include "src/netdb/getnet_internal.h"

#include "hdr/errno_macros.h"
#include "hdr/netdb_macros.h"
#include "hdr/stdint_proxy.h"
#include "hdr/sys_socket_macros.h"
#include "hdr/types/size_t.h"
#include "hdr/types/struct_netent.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/CPP/scope.h"
#include "src/__support/CPP/span.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/CPP/stringstream.h"
#include "src/__support/File/file.h"
#include "src/__support/ctype_utils.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include "src/__support/memory_size.h"
#include "src/__support/netdb/line_reader.h"
#include "src/__support/str_to_integer.h"

namespace LIBC_NAMESPACE_DECL {

static constexpr const char *DEFAULT_NETWORKS_PATH = "/etc/networks";

namespace {

struct LineScan {
  size_t token_count = 0;
  bool matched = false;
};

struct NetQuery {
  cpp::optional<cpp::string_view> name;
  uint32_t net = 0;
};

} // namespace

static cpp::optional<uint8_t> parse_octet(cpp::string_view part) {
  if (part.empty() || !internal::isdigit(part.front()))
    return cpp::nullopt;

  auto parsed = internal::strtointeger<uint8_t>(part.data(), 10, part.size());
  if (parsed.has_error() ||
      static_cast<size_t>(parsed.parsed_len) != part.size())
    return cpp::nullopt;

  return parsed.value;
}

static cpp::optional<uint32_t>
parse_darpa_net(const cpp::StringStream &num_out) {
  if (num_out.overflow())
    return cpp::nullopt;

  cpp::string_view raw = num_out.str();
  if (raw.empty())
    return cpp::nullopt;

  cpp::string_view sv = raw.substr(0, raw.size() - 1);
  uint32_t net = 0;

  for (size_t i = 0; i < 4; ++i) {
    const size_t DOT = sv.find_first_of('.');
    const cpp::optional<uint8_t> OCTET = parse_octet(sv.substr(0, DOT));
    if (!OCTET.has_value())
      return cpp::nullopt;

    net = (net << 8) | *OCTET;
    if (DOT == cpp::string_view::npos)
      return net;

    sv = sv.substr(DOT + 1);
  }

  return cpp::nullopt;
}

static ErrorOr<LineScan> scan_line_tokens(netdb::LineReader &reader,
                                          cpp::string_view target_name,
                                          cpp::StringStream &out,
                                          cpp::StringStream &num_out) {
  reader.begin_line();
  LineScan scan{};

  while (true) {
    cpp::StringStream *dst = &out;
    cpp::string_view tok_target = target_name;
    if (scan.token_count == 1) {
      dst = &num_out;
      tok_target = {};
    }

    ErrorOr<cpp::optional<bool>> tok_or = reader.read_token(*dst, tok_target);
    if (!tok_or.has_value())
      return Error(tok_or.error());
    if (!tok_or->has_value())
      return scan;

    if (**tok_or)
      scan.matched = true;
    ++scan.token_count;
  }
}

static int pack_netent(const cpp::StringStream &out, cpp::span<char> buffer,
                       uint32_t net_num, size_t alias_count,
                       struct netent *result_buf, struct netent **result) {
  if (out.overflow())
    return ERANGE;

  cpp::span<char> tail = buffer.subspan(out.str().size());
  const uintptr_t TAIL_ADDR = reinterpret_cast<uintptr_t>(tail.data());
  const size_t PADDING =
      internal::SafeMemSize::offset_to(TAIL_ADDR, alignof(char *));
  if (tail.size() < PADDING)
    return ERANGE;

  tail = tail.subspan(PADDING);
  if (tail.size() / sizeof(char *) < alias_count + 1)
    return ERANGE;

  char **aliases = reinterpret_cast<char **>(tail.data());
  char *cursor = buffer.data();
  result_buf->n_name = cursor;

  for (size_t i = 0; i < alias_count; ++i) {
    while (*cursor != '\0')
      ++cursor;
    ++cursor;
    aliases[i] = cursor;
  }

  aliases[alias_count] = nullptr;
  result_buf->n_aliases = aliases;
  result_buf->n_addrtype = AF_INET;
  result_buf->n_net = net_num;
  *result = result_buf;
  return 0;
}

static ErrorOr<cpp::optional<int>>
parse_line(netdb::LineReader &reader, NetQuery query, cpp::span<char> buffer,
           struct netent *result_buf, struct netent **result, int *h_errnop) {
  constexpr size_t NUM_BUF_SIZE = 32;
  char num_buf[NUM_BUF_SIZE];
  cpp::StringStream out(buffer);
  cpp::StringStream num_out(num_buf);

  ErrorOr<LineScan> scan_or = scan_line_tokens(
      reader, query.name.value_or(cpp::string_view{}), out, num_out);
  if (!scan_or.has_value())
    return Error(scan_or.error());

  if (scan_or->token_count == 0) {
    if (!reader.has_bytes()) {
      *h_errnop = HOST_NOT_FOUND;
      return cpp::optional<int>(0);
    }
    return cpp::optional<int>(cpp::nullopt);
  }
  if (reader.is_invalid())
    return cpp::optional<int>(cpp::nullopt);
  if (query.name.has_value() && !scan_or->matched)
    return cpp::optional<int>(cpp::nullopt);

  const cpp::optional<uint32_t> NET_NUM = parse_darpa_net(num_out);
  if (!NET_NUM.has_value())
    return cpp::optional<int>(cpp::nullopt);
  if (!query.name.has_value() && *NET_NUM != query.net)
    return cpp::optional<int>(cpp::nullopt);

  const int RC = pack_netent(out, buffer, *NET_NUM, scan_or->token_count - 2,
                             result_buf, result);
  if (RC != 0)
    *h_errnop = NO_RECOVERY;
  return cpp::optional<int>(RC);
}

static int search_networks(File *file, NetQuery query, cpp::span<char> buffer,
                           struct netent *result_buf, struct netent **result,
                           int *h_errnop) {
  netdb::LineReader reader(file);

  while (true) {
    ErrorOr<cpp::optional<int>> line_or =
        parse_line(reader, query, buffer, result_buf, result, h_errnop);
    if (!line_or.has_value()) {
      *h_errnop = NO_RECOVERY;
      return line_or.error();
    }
    if (line_or->has_value())
      return **line_or;
  }
}

static int lookup_net(NetQuery query, struct netent *result_buf,
                      cpp::span<char> buffer, struct netent **result,
                      int *h_errnop, const char *path) {
  *result = nullptr;

  const char *db_path = path != nullptr ? path : DEFAULT_NETWORKS_PATH;
  auto file_or = openfile(db_path, "re");
  if (!file_or.has_value()) {
    if (file_or.error() == ENOENT) {
      *h_errnop = HOST_NOT_FOUND;
      return 0;
    }
    *h_errnop = NO_RECOVERY;
    return file_or.error();
  }

  File *file = file_or.value();
  cpp::scope_exit close_file([file]() { file->close(); });
  File::FileLock lock(file);

  return search_networks(file, query, buffer, result_buf, result, h_errnop);
}

int netdb::lookup_net_by_name(cpp::string_view name, struct netent *result_buf,
                              cpp::span<char> buffer, struct netent **result,
                              int *h_errnop, const char *path) {
  return lookup_net({name, 0}, result_buf, buffer, result, h_errnop, path);
}

int netdb::lookup_net_by_addr(uint32_t net, int type, struct netent *result_buf,
                              cpp::span<char> buffer, struct netent **result,
                              int *h_errnop, const char *path) {
  if (type != AF_INET) {
    *result = nullptr;
    *h_errnop = HOST_NOT_FOUND;
    return 0;
  }

  return lookup_net({cpp::nullopt, net}, result_buf, buffer, result, h_errnop,
                    path);
}

} // namespace LIBC_NAMESPACE_DECL
