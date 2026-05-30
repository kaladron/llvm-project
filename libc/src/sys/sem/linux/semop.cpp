//===-- Linux implementation of semop -------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/sys/sem/semop.h"

#include "src/__support/OSUtil/syscall.h"
#include "src/__support/common.h"
#include "src/__support/libc_errno.h"
#include <sys/syscall.h>
#if !defined(SYS_semop) && defined(__i386__)
#ifndef SYS_ipc
#define SYS_ipc 117
#endif
#define IPCOP_semop 1
#endif

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(int, semop, (int semid, struct sembuf *sops, size_t nsops)) {
#if !defined(SYS_semop) && defined(__i386__)
  int ret = LIBC_NAMESPACE::syscall_impl<int>(
      SYS_ipc, IPCOP_semop, semid, static_cast<long>(nsops), 0L,
      reinterpret_cast<long>(sops), 0L);
#else
  int ret = LIBC_NAMESPACE::syscall_impl<int>(SYS_semop, semid, sops, nsops);
#endif
  if (ret < 0) {
    libc_errno = -ret;
    return -1;
  }
  return ret;
}

} // namespace LIBC_NAMESPACE_DECL
