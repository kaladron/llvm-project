//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Streaming flat-file token and comment reader for netdb databases.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_NETDB_LINE_READER_H
#define LLVM_LIBC_SRC___SUPPORT_NETDB_LINE_READER_H

#include "hdr/types/size_t.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/CPP/stringstream.h"
#include "src/__support/File/file.h"
#include "src/__support/ctype_utils.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace netdb {

struct TokenMatcher {
  cpp::string_view target;
  size_t len = 0;
  bool case_insensitive = false;
  bool matches = true;

  void push(char ch) {
    if (len >= target.size()) {
      matches = false;
    } else {
      char lhs = ch;
      char rhs = target[len];
      if (case_insensitive) {
        lhs = internal::tolower(lhs);
        rhs = internal::tolower(rhs);
      }
      if (lhs != rhs)
        matches = false;
    }
    ++len;
  }

  bool matched() const { return matches && len == target.size(); }
};

class LineReader {
  File *file;
  bool saw_bytes = false;
  bool at_eol = false;
  bool at_eof = false;
  bool in_comment = false;
  bool invalid_line = false;

  ErrorOr<cpp::optional<char>> next_char() {
    while (true) {
      char ch = '\0';
      FileIOResult res = file->read_unlocked(&ch, 1);
      if (res.has_error())
        return Error(res.error);
      if (res.value == 0) {
        at_eof = true;
        at_eol = true;
        return cpp::optional<char>(cpp::nullopt);
      }

      saw_bytes = true;
      if (ch == '\n') {
        at_eol = true;
        return cpp::optional<char>(cpp::nullopt);
      }
      if (ch == '#')
        in_comment = true;
      if (in_comment)
        continue;
      if (ch == '\0')
        invalid_line = true;
      return cpp::optional<char>(ch);
    }
  }

public:
  explicit LineReader(File *f) : file(f) {}

  void begin_line() {
    saw_bytes = false;
    at_eol = at_eof;
    in_comment = false;
    invalid_line = false;
  }

  bool has_bytes() const { return saw_bytes; }

  bool is_invalid() const { return invalid_line; }

  ErrorOr<cpp::optional<bool>> read_token(cpp::StringStream &dst,
                                          cpp::string_view target,
                                          bool case_insensitive = false) {
    TokenMatcher matcher{target, 0, case_insensitive, true};
    bool in_token = false;

    while (!at_eol) {
      ErrorOr<cpp::optional<char>> ch_or = next_char();
      if (!ch_or.has_value())
        return Error(ch_or.error());
      if (!ch_or->has_value())
        break;

      const char CH = **ch_or;
      if (internal::isspace(CH)) {
        if (in_token)
          break;
        continue;
      }

      in_token = true;
      matcher.push(CH);
      dst << CH;
    }

    if (!in_token)
      return cpp::optional<bool>(cpp::nullopt);

    dst << cpp::StringStream::ENDS;
    return cpp::optional<bool>(matcher.matched());
  }
};

} // namespace netdb
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_NETDB_LINE_READER_H
