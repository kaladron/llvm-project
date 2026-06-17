//===-- Linux implementation of clock_nanosleep wrapper ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SYSCALL_WRAPPERS_CLOCK_NANOSLEEP_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SYSCALL_WRAPPERS_CLOCK_NANOSLEEP_H

#include "hdr/types/clockid_t.h"
#include "hdr/types/struct_timespec.h"
#include "src/__support/OSUtil/linux/syscall.h" // For syscall_checked
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include <sys/syscall.h> // For syscall numbers

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {

LIBC_INLINE ErrorOr<int> clock_nanosleep(clockid_t clockid, int flags,
                                         const struct timespec *req,
                                         struct timespec *rem) {
#if defined(SYS_clock_nanosleep)
  return syscall_checked<int>(SYS_clock_nanosleep, clockid, flags, req, rem);
#elif defined(SYS_clock_nanosleep_time64)
  return syscall_checked<int>(SYS_clock_nanosleep_time64, clockid, flags, req,
                              rem);
#else
#error "clock_nanosleep and clock_nanosleep_time64 syscalls not available."
#endif
}

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SYSCALL_WRAPPERS_CLOCK_NANOSLEEP_H
