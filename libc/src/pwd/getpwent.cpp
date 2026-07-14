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
#include "src/__support/File/linux/file.h" // For create_file_from_fd
#include "src/__support/OSUtil/linux/syscall_wrappers/close.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/open.h"
#include "src/__support/common.h"
#include "src/__support/libc_errno.h"
#include "src/__support/macros/config.h"
#include "src/pwd/pwd_utils.h"

#include "hdr/fcntl_macros.h" // For O_RDONLY, O_CLOEXEC
#include "hdr/stdio_macros.h" // For SEEK_SET

#ifndef LIBC_COPT_PWD_FILE_PATH
#define LIBC_COPT_PWD_FILE_PATH "/etc/passwd"
#endif

namespace LIBC_NAMESPACE_DECL {

static File *pwd_file = nullptr;
static const char *pwd_file_path = LIBC_COPT_PWD_FILE_PATH;
static char line_buffer[1024];
static struct passwd pwd_entry;

namespace internal {
void set_passwd_path(const char *path) {
  if (pwd_file) {
    pwd_file->close();
    pwd_file = nullptr;
  }
  pwd_file_path = path;
}
} // namespace internal

int setpwent_impl() {
  if (!pwd_file) {
    auto fd = LIBC_NAMESPACE::linux_syscalls::open(pwd_file_path,
                                                   O_RDONLY | O_CLOEXEC, 0);
    if (!fd.has_value())
      return fd.error();
    auto result = LIBC_NAMESPACE::create_file_from_fd(fd.value(), "r");
    if (!result.has_value()) {
      LIBC_NAMESPACE::linux_syscalls::close(fd.value());
      return result.error();
    }
    pwd_file = result.value();
  } else {
    auto result = pwd_file->seek(0, SEEK_SET);
    if (!result.has_value())
      return result.error();
  }
  return 0;
}

int endpwent_impl() {
  if (pwd_file) {
    int result = pwd_file->close();
    pwd_file = nullptr;
    return result;
  }
  return 0;
}

struct ReadLineResult {
  size_t bytes_read;
  bool truncated;
};

static ErrorOr<ReadLineResult> read_line(File *f, char *buf, size_t max_len) {
  if (max_len < 1)
    return Error(EINVAL);

  unsigned char c = '\0';
  f->lock();
  size_t i = 0;
  FileIOResult result(0);
  bool truncated = false;
  for (; i < (max_len - 1); ++i) {
    result = f->read_unlocked(&c, 1);
    if (result.has_error()) {
      f->unlock();
      return Error(result.error);
    }
    if (result.value != 1)
      break;
    buf[i] = c;
    if (c == '\n') {
      i++; // Include '\n' in bytes_read count
      break;
    }
  }

  // Check for truncation
  if (i == max_len - 1 && c != '\n') {
    truncated = true;
    // Discard the rest of the line
    while (true) {
      result = f->read_unlocked(&c, 1);
      if (result.has_error()) {
        f->unlock();
        return Error(result.error);
      }
      if (result.value != 1 || c == '\n')
        break;
    }
  }

  bool has_error = f->error_unlocked();
  bool has_eof = f->iseof_unlocked();
  f->unlock();

  if (has_error)
    return Error(EIO);

  if (i == 0 && has_eof)
    return ReadLineResult{0, false};

  buf[i] = '\0';
  return ReadLineResult{i, truncated};
}

LLVM_LIBC_FUNCTION(struct passwd *, getpwent, ()) {
  if (!pwd_file) {
    auto fd = LIBC_NAMESPACE::linux_syscalls::open(pwd_file_path,
                                                   O_RDONLY | O_CLOEXEC, 0);
    if (!fd.has_value()) {
      libc_errno = fd.error();
      return nullptr;
    }
    auto result = LIBC_NAMESPACE::create_file_from_fd(fd.value(), "r");
    if (!result.has_value()) {
      LIBC_NAMESPACE::linux_syscalls::close(fd.value());
      libc_errno = result.error();
      return nullptr;
    }
    pwd_file = result.value();
  }

  while (true) {
    auto result = read_line(pwd_file, line_buffer, sizeof(line_buffer));
    if (!result.has_value()) {
      libc_errno = result.error();
      return nullptr;
    }

    ReadLineResult res = result.value();
    if (res.bytes_read == 0)
      return nullptr;

    if (res.truncated) {
      libc_errno = EINVAL;
      return nullptr;
    }

    // Remove newline
    size_t len = res.bytes_read;
    if (len > 0 && line_buffer[len - 1] == '\n')
      line_buffer[len - 1] = '\0';

    if (internal::parse_passwd_line(line_buffer, &pwd_entry))
      return &pwd_entry;

    libc_errno = EINVAL;
    return nullptr;
  }
}

} // namespace LIBC_NAMESPACE_DECL

