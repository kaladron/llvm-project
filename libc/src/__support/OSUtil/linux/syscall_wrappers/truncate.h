//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Linux implementation of truncate wrapper.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SYSCALL_WRAPPERS_TRUNCATE_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SYSCALL_WRAPPERS_TRUNCATE_H

#include "hdr/stdint_proxy.h" // For uint64_t
#include "hdr/types/off_t.h"
#include "src/__support/OSUtil/linux/syscall.h" // For syscall_checked
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include <sys/syscall.h> // For syscall numbers

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {

#ifdef SYS_truncate
LIBC_INLINE ErrorOr<int> truncate(const char *path, off_t len) {
  return syscall_checked<int>(SYS_truncate, path, len);
}
#elif defined(SYS_truncate64)
LIBC_INLINE ErrorOr<int> truncate(const char *path, off_t len) {
  static_assert(sizeof(off_t) == 8);
  return syscall_checked<int>(SYS_truncate64, path, (long)len,
                              (long)(((uint64_t)(len)) >> 32));
}
#endif

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SYSCALL_WRAPPERS_TRUNCATE_H
