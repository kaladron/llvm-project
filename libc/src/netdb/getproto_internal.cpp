//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of shared /etc/protocols lookup helpers.
///
//===----------------------------------------------------------------------===//

#include "src/netdb/getproto_internal.h"

#include "hdr/errno_macros.h"
#include "hdr/stdint_proxy.h"
#include "hdr/types/size_t.h"
#include "hdr/types/struct_protoent.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/CPP/scope.h"
#include "src/__support/CPP/span.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/File/file.h"
#include "src/__support/File/line_reader.h"
#include "src/__support/ctype_utils.h"
#include "src/__support/error_or.h"
#include "src/__support/libc_assert.h"
#include "src/__support/macros/config.h"
#include "src/__support/memory_size.h"
#include "src/__support/str_to_integer.h"

namespace LIBC_NAMESPACE_DECL {

static constexpr const char *DEFAULT_PROTOCOLS_PATH = "/etc/protocols";

// Extracts the next whitespace-delimited token from a NUL-terminated or empty
// span and NUL-terminates the token in place.
static constexpr cpp::optional<cpp::span<char>>
next_token(cpp::span<char> &rest) {
  while (rest.size() > 1 && internal::isspace(rest.front()))
    rest = rest.subspan(1);
  if (rest.size() <= 1)
    return cpp::nullopt;

  size_t len = 0;
  while (len + 1 < rest.size() && !internal::isspace(rest[len]))
    ++len;

  rest[len] = '\0';
  cpp::span<char> token = rest.first(len);
  rest = rest.subspan(len + 1);
  return token;
}

// Parses a NUL-terminated protocol record (`line.back() == '\0'`) in place,
// storing the `p_aliases` pointer array in `scratch`. Returns true on a valid
// record, false on a blank or malformed line, or Error(ERANGE) if `scratch`
// cannot hold the alias pointer array.
static ErrorOr<bool> parse_proto_line(cpp::span<char> line,
                                      cpp::span<char> scratch,
                                      struct protoent &entry) {
  cpp::span<char> rest = line;
  auto name = next_token(rest);
  if (!name)
    return false;

  auto proto_tok = next_token(rest);
  if (!proto_tok || !internal::isdigit(proto_tok->front()))
    return false;
  auto proto_num =
      internal::strtointeger<int>(proto_tok->data(), 10, proto_tok->size());
  if (proto_num.has_error() ||
      static_cast<size_t>(proto_num.parsed_len) != proto_tok->size())
    return false;

  const uintptr_t ADDR = reinterpret_cast<uintptr_t>(scratch.data());
  const size_t PAD = internal::SafeMemSize::offset_to(ADDR, alignof(char *));
  if (scratch.size() < PAD || scratch.size() - PAD < sizeof(char *))
    return Error(ERANGE);
  scratch = scratch.subspan(PAD);

  cpp::span<char *> aliases(reinterpret_cast<char **>(scratch.data()),
                            scratch.size() / sizeof(char *));
  size_t alias_count = 0;
  while (auto alias = next_token(rest)) {
    if (alias_count + 1 >= aliases.size())
      return Error(ERANGE);
    aliases[alias_count++] = alias->data();
  }
  aliases[alias_count] = nullptr;

  entry.p_name = name->data();
  entry.p_aliases = aliases.data();
  entry.p_proto = proto_num.value;
  return true;
}

static ErrorOr<bool> read_next_proto(File *file, struct protoent &entry,
                                     cpp::span<char> buffer) {
  LIBC_ASSERT(file != nullptr);

  if (buffer.size() < 2)
    return Error(ERANGE);

  cpp::span<char> line_cap = buffer.first(buffer.size() - 1);
  while (true) {
    auto read_or = read_line(file, line_cap);
    if (!read_or.has_value())
      return Error(read_or.error());

    ReadLineResult res = *read_or;
    if (res.eof)
      return false;
    if (res.bytes_read == 0)
      continue;

    cpp::string_view raw(buffer.data(), res.bytes_read);
    const size_t COMMENT_POS = raw.find_first_of('#');
    if (res.truncated && COMMENT_POS == cpp::string_view::npos)
      return Error(ERANGE);
    if (COMMENT_POS != cpp::string_view::npos)
      raw = raw.substr(0, COMMENT_POS);
    if (raw.empty() || raw.contains('\0'))
      continue;

    buffer[raw.size()] = '\0';
    auto parsed = parse_proto_line(buffer.first(raw.size() + 1),
                                   buffer.subspan(raw.size() + 1), entry);
    if (!parsed.has_value())
      return Error(parsed.error());
    if (*parsed)
      return true;
  }
}

static constexpr bool matches_name(const struct protoent &entry,
                                   cpp::string_view name) {
  if (cpp::string_view(entry.p_name) == name)
    return true;
  for (size_t i = 0; entry.p_aliases[i] != nullptr; ++i) {
    if (cpp::string_view(entry.p_aliases[i]) == name)
      return true;
  }
  return false;
}

ErrorOr<bool> netdb::lookup_proto_by_name(cpp::string_view name,
                                          struct protoent &entry,
                                          cpp::span<char> buffer,
                                          const char *path) {
  const char *db_path = path != nullptr ? path : DEFAULT_PROTOCOLS_PATH;
  auto file_or = openfile(db_path, "re");
  if (!file_or.has_value()) {
    if (file_or.error() == ENOENT)
      return false;
    return Error(file_or.error());
  }

  File *file = *file_or;
  cpp::scope_exit close_file([file]() { file->close(); });

  while (true) {
    auto next_or = read_next_proto(file, entry, buffer);
    if (!next_or.has_value())
      return Error(next_or.error());
    if (!*next_or)
      return false;
    if (matches_name(entry, name))
      return true;
  }
}

} // namespace LIBC_NAMESPACE_DECL
