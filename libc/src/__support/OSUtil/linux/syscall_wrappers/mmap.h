//===-- Implementation header for mmap ---------------------------*- C++
//-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_MMAP_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_MMAP_H

#include "hdr/types/off_t.h"
#include "src/__support/OSUtil/linux/syscall.h" // syscall_impl, linux_utils::is_valid_mmap
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include <linux/param.h> // For EXEC_PAGESIZE
#include <sys/syscall.h> // For syscall numbers

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {

LIBC_INLINE ErrorOr<void *> mmap(void *addr, size_t size, int prot, int flags,
                                 int fd, off_t offset) {
#ifdef SYS_mmap2
  offset /= EXEC_PAGESIZE;
  long syscall_number = SYS_mmap2;
#elif defined(SYS_mmap)
  long syscall_number = SYS_mmap;
#else
#error "mmap or mmap2 syscalls not available."
#endif

  long ret =
      syscall_impl<long>(syscall_number, reinterpret_cast<long>(addr), size,
                         prot, flags, fd, static_cast<long>(offset));

  if (!linux_utils::is_valid_mmap(ret))
    return Error(-static_cast<int>(ret));
  return reinterpret_cast<void *>(ret);
}

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_MMAP_H
