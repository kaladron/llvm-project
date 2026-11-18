//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Shared /etc/services streaming parser and reentrant lookup helpers.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_NETDB_SERVICES_H
#define LLVM_LIBC_SRC___SUPPORT_NETDB_SERVICES_H

#include "hdr/errno_macros.h"
#include "hdr/stdint_proxy.h"
#include "hdr/types/in_port_t.h"
#include "hdr/types/size_t.h"
#include "hdr/types/struct_servent.h"
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
#include "src/__support/memory_size.h"
#include "src/__support/net/byte_order.h"
#include "src/__support/netdb/line_reader.h"
#include "src/__support/str_to_integer.h"

namespace LIBC_NAMESPACE_DECL {
namespace netdb {

#ifndef LIBC_COPT_PUBLIC_PACKAGING
LIBC_INLINE_VAR LIBC_THREAD_LOCAL const char *services_path = nullptr;

LIBC_INLINE void set_services_path(const char *path) { services_path = path; }

LIBC_INLINE const char *get_services_path() { return services_path; }
#else
LIBC_ALWAYS_INLINE constexpr const char *get_services_path() { return nullptr; }
#endif // LIBC_COPT_PUBLIC_PACKAGING

struct PortProto {
  in_port_t net_port = 0;
  cpp::string_view proto;
};

struct ServiceLineScan {
  size_t token_count = 0;
};

LIBC_INLINE cpp::optional<PortProto>
parse_port_proto(const cpp::StringStream &port_proto_out,
                 const char *req_proto) {
  if (port_proto_out.overflow())
    return cpp::nullopt;

  cpp::string_view raw = port_proto_out.str();
  if (raw.empty())
    return cpp::nullopt;

  cpp::string_view token = raw.substr(0, raw.size() - 1);
  const size_t SLASH = token.find_first_of('/');
  if (SLASH == cpp::string_view::npos)
    return cpp::nullopt;

  cpp::string_view port_part = token.substr(0, SLASH);
  cpp::string_view proto_part = token.substr(SLASH + 1);
  if (port_part.empty() || proto_part.empty())
    return cpp::nullopt;
  if (!internal::isdigit(port_part.front()))
    return cpp::nullopt;
  if (req_proto != nullptr) {
    if (proto_part != req_proto)
      return cpp::nullopt;
  }

  auto parsed =
      internal::strtointeger<uint16_t>(port_part.data(), 10, port_part.size());
  if (parsed.has_error() ||
      static_cast<size_t>(parsed.parsed_len) != port_part.size())
    return cpp::nullopt;

  return PortProto{net::hton(static_cast<in_port_t>(parsed.value)), proto_part};
}

LIBC_INLINE ErrorOr<ServiceLineScan>
scan_service_tokens(LineReader &reader, cpp::StringStream &out,
                    cpp::StringStream &port_proto_out) {
  reader.begin_line();
  ServiceLineScan scan{};

  while (true) {
    cpp::StringStream *dst = &out;
    if (scan.token_count == 1)
      dst = &port_proto_out;

    ErrorOr<cpp::optional<bool>> tok_or = reader.read_token(*dst, {});
    if (!tok_or.has_value())
      return Error(tok_or.error());
    if (!tok_or->has_value())
      return scan;

    ++scan.token_count;
  }
}

LIBC_INLINE int pack_servent(cpp::StringStream &out, cpp::span<char> buffer,
                             PortProto port_proto, size_t alias_count,
                             struct servent *result_buf,
                             struct servent **result) {
  out << port_proto.proto << cpp::StringStream::ENDS;
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
  result_buf->s_name = cursor;

  for (size_t i = 0; i < alias_count; ++i) {
    while (*cursor != '\0')
      ++cursor;
    ++cursor;
    aliases[i] = cursor;
  }

  while (*cursor != '\0')
    ++cursor;
  ++cursor;
  result_buf->s_proto = cursor;

  aliases[alias_count] = nullptr;
  result_buf->s_aliases = aliases;
  result_buf->s_port = static_cast<int>(port_proto.net_port);
  *result = result_buf;
  return 0;
}

LIBC_INLINE ErrorOr<cpp::optional<int>>
parse_service_line_by_port(LineReader &reader, int port, const char *req_proto,
                           cpp::span<char> buffer, struct servent *result_buf,
                           struct servent **result) {
  constexpr size_t PORT_PROTO_BUF_SIZE = 64;
  char port_proto_buf[PORT_PROTO_BUF_SIZE];
  cpp::StringStream out(buffer);
  cpp::StringStream port_proto_out(port_proto_buf);

  ErrorOr<ServiceLineScan> scan_or =
      scan_service_tokens(reader, out, port_proto_out);
  if (!scan_or.has_value())
    return Error(scan_or.error());

  if (scan_or->token_count == 0) {
    if (!reader.has_bytes())
      return cpp::optional<int>(0);
    return cpp::optional<int>(cpp::nullopt);
  }
  if (reader.is_invalid())
    return cpp::optional<int>(cpp::nullopt);

  const cpp::optional<PortProto> PP =
      parse_port_proto(port_proto_out, req_proto);
  if (!PP.has_value() || static_cast<int>(PP->net_port) != port)
    return cpp::optional<int>(cpp::nullopt);

  return cpp::optional<int>(pack_servent(
      out, buffer, *PP, scan_or->token_count - 2, result_buf, result));
}

LIBC_INLINE int lookup_serv_by_port(int port, const char *proto,
                                    struct servent *result_buf,
                                    cpp::span<char> buffer,
                                    struct servent **result,
                                    const char *path = nullptr) {
  static constexpr const char *DEFAULT_SERVICES_PATH = "/etc/services";
  *result = nullptr;

  const char *db_path = path != nullptr ? path : DEFAULT_SERVICES_PATH;
  auto file_or = openfile(db_path, "re");
  if (!file_or.has_value())
    return file_or.error() == ENOENT ? 0 : file_or.error();

  File *file = file_or.value();
  cpp::scope_exit close_file([file]() { file->close(); });
  File::FileLock lock(file);

  LineReader reader(file);
  while (true) {
    ErrorOr<cpp::optional<int>> line_or = parse_service_line_by_port(
        reader, port, proto, buffer, result_buf, result);
    if (!line_or.has_value())
      return line_or.error();
    if (line_or->has_value())
      return **line_or;
  }
}

} // namespace netdb
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_NETDB_SERVICES_H
