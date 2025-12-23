//===-- Implementation of getpwent ----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/pwd/getpwent.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/libc_errno.h"
#include "src/stdio/fclose.h"
#include "src/stdio/fgets.h"
#include "src/stdio/fopen.h"
#include "src/stdlib/atoi.h"
#include "src/string/strchr.h"
#include "src/string/strcpy.h"
#include "src/string/strlen.h"
#include "src/string/strtok.h"

#include "src/stdio/fseek.h"
#include "hdr/stdio_macros.h"

namespace LIBC_NAMESPACE_DECL {

static FILE *pwd_file = nullptr;
static char line_buffer[1024];
static struct passwd pwd_entry;

void setpwent_impl() {
  if (pwd_file) {
    LIBC_NAMESPACE::fseek(pwd_file, 0, SEEK_SET);
  }
}

void endpwent_impl() {
  if (pwd_file) {
    LIBC_NAMESPACE::fclose(pwd_file);
    pwd_file = nullptr;
  }
}

struct passwd *getpwent() {
  if (!pwd_file) {
    pwd_file = LIBC_NAMESPACE::fopen("/etc/passwd", "r");
    if (!pwd_file)
      return nullptr;
  }

  if (!LIBC_NAMESPACE::fgets(line_buffer, sizeof(line_buffer), pwd_file))
    return nullptr;

  // Remove newline
  size_t len = LIBC_NAMESPACE::strlen(line_buffer);
  if (len > 0 && line_buffer[len - 1] == '\n')
    line_buffer[len - 1] = '\0';

  // Parse line
  // Format: name:passwd:uid:gid:gecos:dir:shell
  char *ptr = line_buffer;
  
  // name
  pwd_entry.pw_name = ptr;
  ptr = LIBC_NAMESPACE::strchr(ptr, ':');
  if (!ptr) return nullptr;
  *ptr++ = '\0';

  // passwd
  pwd_entry.pw_passwd = ptr;
  ptr = LIBC_NAMESPACE::strchr(ptr, ':');
  if (!ptr) return nullptr;
  *ptr++ = '\0';

  // uid
  char *uid_str = ptr;
  ptr = LIBC_NAMESPACE::strchr(ptr, ':');
  if (!ptr) return nullptr;
  *ptr++ = '\0';
  pwd_entry.pw_uid = LIBC_NAMESPACE::atoi(uid_str);

  // gid
  char *gid_str = ptr;
  ptr = LIBC_NAMESPACE::strchr(ptr, ':');
  if (!ptr) return nullptr;
  *ptr++ = '\0';
  pwd_entry.pw_gid = LIBC_NAMESPACE::atoi(gid_str);

  // gecos
  pwd_entry.pw_gecos = ptr;
  ptr = LIBC_NAMESPACE::strchr(ptr, ':');
  if (!ptr) return nullptr;
  *ptr++ = '\0';

  // dir
  pwd_entry.pw_dir = ptr;
  ptr = LIBC_NAMESPACE::strchr(ptr, ':');
  if (!ptr) return nullptr;
  *ptr++ = '\0';

  // shell
  pwd_entry.pw_shell = ptr;

  return &pwd_entry;
}

} // namespace LIBC_NAMESPACE_DECL
