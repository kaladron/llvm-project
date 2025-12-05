//===-- Implementation of regerror ------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/regex/regerror.h"
#include "src/__support/CPP/array.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/string/memory_utils/inline_memcpy.h"

namespace LIBC_NAMESPACE_DECL {

namespace {

// Error message strings for each error code.
// These are required by POSIX to be implementation-defined.
struct ErrorMessage {
  int code;
  cpp::string_view message;
};

// clang-format off
constexpr cpp::array<ErrorMessage, 14> ERROR_MESSAGES = {{
    {0,            "Success"},
    {REG_NOMATCH,  "No match"},
    {REG_BADPAT,   "Invalid regular expression"},
    {REG_ECOLLATE, "Invalid collating element"},
    {REG_ECTYPE,   "Invalid character class"},
    {REG_EESCAPE,  "Trailing backslash"},
    {REG_ESUBREG,  "Invalid back reference"},
    {REG_EBRACK,   "Unmatched ["},
    {REG_EPAREN,   "Unmatched ("},
    {REG_EBRACE,   "Unmatched {"},
    {REG_BADBR,    "Invalid content of {}"},
    {REG_ERANGE,   "Invalid range end"},
    {REG_ESPACE,   "Memory exhausted"},
    {REG_BADRPT,   "Invalid preceding regular expression"},
}};
// clang-format on

LIBC_INLINE cpp::string_view get_error_message(int errcode) {
  for (const auto &entry : ERROR_MESSAGES) {
    if (entry.code == errcode)
      return entry.message;
  }
  return "Unknown error";
}

} // namespace

LLVM_LIBC_FUNCTION(size_t, regerror,
                   (int errcode, const regex_t *__restrict preg,
                    char *__restrict errbuf, size_t errbuf_size)) {
  // preg may be null; if so, we still provide the error message.
  (void)preg;

  cpp::string_view message = get_error_message(errcode);
  size_t needed = message.size() + 1; // Include null terminator.

  if (errbuf_size > 0 && errbuf != nullptr) {
    size_t copy_len =
        (message.size() < errbuf_size) ? message.size() : errbuf_size - 1;
    inline_memcpy(errbuf, message.data(), copy_len);
    errbuf[copy_len] = '\0';
  }

  return needed;
}

} // namespace LIBC_NAMESPACE_DECL
