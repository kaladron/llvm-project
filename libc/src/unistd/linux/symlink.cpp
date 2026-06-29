//===-- Linux implementation of symlink -----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/unistd/symlink.h"

#include "src/__support/common.h"
#include "src/__support/libc_errno.h"
#include "src/__support/macros/config.h"

#include "hdr/fcntl_macros.h"
#include <sys/syscall.h> // For syscall numbers.

#ifdef SYS_symlinkat
#include "src/__support/OSUtil/linux/syscall_wrappers/symlinkat.h"
#elif defined(SYS_symlink)
#include "src/__support/OSUtil/linux/syscall_wrappers/symlink.h"
#endif

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(int, symlink, (const char *path1, const char *path2)) {
#ifdef SYS_symlinkat
  auto result = linux_syscalls::symlinkat(path1, AT_FDCWD, path2);
#elif defined(SYS_symlink)
  auto result = linux_syscalls::symlink(path1, path2);
#else
#error "symlink or symlinkat syscalls not available."
#endif
  if (!result) {
    libc_errno = result.error();
    return -1;
  }
  return result.value();
}

} // namespace LIBC_NAMESPACE_DECL
