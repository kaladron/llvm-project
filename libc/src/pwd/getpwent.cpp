//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of getpwent.
///
//===----------------------------------------------------------------------===//

#include "src/pwd/getpwent.h"
#include "src/__support/File/file.h"
#include "src/__support/common.h"
#include "src/__support/libc_errno.h"
#include "src/__support/macros/config.h"
#include "src/pwd/pwd_utils.h"
#include "src/string/string_utils.h"

#include "hdr/stdio_macros.h"

namespace LIBC_NAMESPACE_DECL {

static File *pwd_file = nullptr;
static char line_buffer[1024];
static struct passwd pwd_entry;

void setpwent_impl() {
  if (pwd_file)
    pwd_file->seek(0, SEEK_SET);
}

void endpwent_impl() {
  if (pwd_file) {
    pwd_file->close();
    pwd_file = nullptr;
  }
}

static bool read_line(File *f, char *buf, size_t max_len) {
  if (max_len < 1)
    return false;

  unsigned char c = '\0';
  f->lock();
  size_t i = 0;
  FileIOResult result(0);
  for (; i < (max_len - 1) && c != '\n'; ++i) {
    result = f->read_unlocked(&c, 1);
    if (result.has_error()) {
      libc_errno = result.error;
      break;
    }
    if (result.value != 1)
      break;
    buf[i] = c;
  }
  bool has_error = f->error_unlocked();
  bool has_eof = f->iseof_unlocked();
  f->unlock();

  if (has_error || (i == 0 && has_eof))
    return false;

  buf[i] = '\0';
  return true;
}

LLVM_LIBC_FUNCTION(struct passwd *, getpwent, ()) {
  if (!pwd_file) {
    auto result = LIBC_NAMESPACE::openfile("/etc/passwd", "r");
    if (!result.has_value()) {
      libc_errno = result.error();
      return nullptr;
    }
    pwd_file = result.value();
  }

  while (read_line(pwd_file, line_buffer, sizeof(line_buffer))) {
    // Remove newline
    size_t len = LIBC_NAMESPACE::internal::string_length(line_buffer);
    if (len > 0 && line_buffer[len - 1] == '\n')
      line_buffer[len - 1] = '\0';

    if (internal::parse_passwd_line(line_buffer, &pwd_entry))
      return &pwd_entry;
  }

  return nullptr;
}

} // namespace LIBC_NAMESPACE_DECL
