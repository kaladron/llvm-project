//===-- Linux implementation of pipe --------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/unistd/pipe.h"

#include "src/__support/common.h"
#include "src/__support/libc_errno.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/sanitizer.h" // for LIBC_MSAN_UNPOISON
#include <sys/syscall.h>                    // For syscall numbers.

#ifdef SYS_pipe
#include "src/__support/OSUtil/linux/syscall_wrappers/pipe.h"
#elif defined(SYS_pipe2)
#include "src/__support/OSUtil/linux/syscall_wrappers/pipe2.h"
#endif

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(int, pipe, (int pipefd[2])) {
#ifdef SYS_pipe
  auto result = linux_syscalls::pipe(pipefd);
#elif defined(SYS_pipe2)
  auto result = linux_syscalls::pipe2(pipefd, 0);
#else
#error "pipe implementation not available for this architecture"
#endif

  if (!result) {
    libc_errno = result.error();
    return -1;
  }
  LIBC_MSAN_UNPOISON(pipefd, sizeof(int) * 2);
  return result.value();
}

} // namespace LIBC_NAMESPACE_DECL
