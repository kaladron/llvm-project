//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Linux implementation of setitimer wrapper.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SYSCALL_WRAPPERS_SETITIMER_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SYSCALL_WRAPPERS_SETITIMER_H

#include "hdr/errno_macros.h"
#include "hdr/types/struct_itimerval.h"
#include "src/__support/CPP/limits.h"
#include "src/__support/OSUtil/linux/syscall.h" // For syscall_checked
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include <sys/syscall.h> // For syscall numbers

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {

LIBC_INLINE ErrorOr<int> setitimer(int which, const struct itimerval *new_value,
                                   struct itimerval *old_value) {
#ifdef SYS_setitimer
  if constexpr (sizeof(time_t) > sizeof(long)) {
    long old_value32[4];
    long *old_value32_ptr = old_value ? old_value32 : nullptr;

    if (new_value) {
      if (new_value->it_interval.tv_sec > cpp::numeric_limits<long>::max() ||
          new_value->it_interval.tv_sec < cpp::numeric_limits<long>::min() ||
          new_value->it_value.tv_sec > cpp::numeric_limits<long>::max() ||
          new_value->it_value.tv_sec < cpp::numeric_limits<long>::min()) {
        return Error(EOVERFLOW);
      }

      long new_value32[4] = {static_cast<long>(new_value->it_interval.tv_sec),
                             static_cast<long>(new_value->it_interval.tv_usec),
                             static_cast<long>(new_value->it_value.tv_sec),
                             static_cast<long>(new_value->it_value.tv_usec)};

      auto ret = syscall_checked<int>(SYS_setitimer, which, new_value32,
                                      old_value32_ptr);
      if (!ret)
        return ret;
    } else {
      auto ret =
          syscall_checked<int>(SYS_setitimer, which, nullptr, old_value32_ptr);
      if (!ret)
        return ret;
    }

    if (old_value) {
      old_value->it_interval.tv_sec = old_value32[0];
      old_value->it_interval.tv_usec = old_value32[1];
      old_value->it_value.tv_sec = old_value32[2];
      old_value->it_value.tv_usec = old_value32[3];
    }
    return 0;
  } else {
    return syscall_checked<int>(SYS_setitimer, which, new_value, old_value);
  }
#else
#error "SYS_setitimer syscall not available."
#endif
}

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SYSCALL_WRAPPERS_SETITIMER_H
