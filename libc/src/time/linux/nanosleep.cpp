//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Linux implementation of nanosleep function.
///
//===----------------------------------------------------------------------===//

#include "src/time/nanosleep.h"
#include "hdr/time_macros.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/clock_nanosleep.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/nanosleep.h"
#include "src/__support/common.h"
#include "src/__support/libc_errno.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(int, nanosleep, (const timespec *req, timespec *rem)) {
#if defined(SYS_clock_nanosleep_time64)
  auto result = linux_syscalls::clock_nanosleep(CLOCK_REALTIME, 0, req, rem);
#elif defined(SYS_nanosleep)
  static_assert(
      sizeof(timespec::tv_nsec) == sizeof(long),
      "This legacy syscall fallback is only safe on platforms where tv_nsec "
      "matches the register size (long). It is unsafe on 32-bit platforms "
      "with 64-bit tv_nsec.");
  auto result = linux_syscalls::nanosleep(req, rem);
#else
#error "SYS_nanosleep and SYS_clock_nanosleep_time64 syscalls not available."
#endif

  if (!result) {
    libc_errno = result.error();
    return -1;
  }
  return result.value();
}

} // namespace LIBC_NAMESPACE_DECL
