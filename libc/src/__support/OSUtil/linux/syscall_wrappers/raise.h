//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation header for raise.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SYSCALL_WRAPPERS_RAISE_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SYSCALL_WRAPPERS_RAISE_H

#include "hdr/signal_macros.h"
#include "hdr/types/sigset_t.h"
#include "src/__support/OSUtil/linux/syscall.h" // syscall_impl
#include "src/__support/OSUtil/linux/syscall_wrappers/getpid.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/gettid.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/rt_sigprocmask.h"
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include <sys/syscall.h> // For syscall numbers

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {
/// Sends a signal to the calling thread.
///
/// \param sig The signal to send.
/// \return A ErrorOr wrapper containing 0 on success, or the error code on failure.
LIBC_INLINE ErrorOr<int> raise(int sig) {
  /// RAII guard to temporarily block all signals.
  /// This ensures that the signal sent by tgkill is delivered to the calling
  /// thread without being interrupted by other signals.
  class SigMaskGuard {
    [[maybe_unused]] sigset_t old_set;
    [[maybe_unused]] ErrorOr<int> &status;

  public:
    LIBC_INLINE SigMaskGuard(ErrorOr<int> &status) : old_set{}, status(status) {
      sigset_t full_set = sigset_t{{-1UL}};
      status = linux_syscalls::rt_sigprocmask(SIG_BLOCK, &full_set, &old_set);
    }
    LIBC_INLINE ~SigMaskGuard() {
      if (status.has_value()) {
        auto restore_result =
            linux_syscalls::rt_sigprocmask(SIG_SETMASK, &old_set, nullptr);
        if (!restore_result.has_value())
          status = restore_result.error();
      }
    }
  };
  ErrorOr<int> status = 0;
  {
    SigMaskGuard sig_mask(status);

    if (!status.has_value())
      return status;

    pid_t pid = linux_syscalls::getpid();
    pid_t tid = linux_syscalls::gettid();

    int result = syscall_impl<int>(SYS_tgkill, pid, tid, sig);
    if (result < 0)
      return Error(-result);
  }
  return status;
}

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SYSCALL_WRAPPERS_RAISE_H
