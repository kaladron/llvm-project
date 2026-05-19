//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Linux implementation of the POSIX timer_settime function.
///
//===----------------------------------------------------------------------===//

#include "src/time/timer_settime.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/timer_settime.h"
#include "src/__support/common.h"
#include "src/__support/libc_errno.h"

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(int, timer_settime,
                   (timer_t timerid, int flags,
                    const struct itimerspec *__restrict new_value,
                    struct itimerspec *__restrict old_value)) {
  auto result = linux_syscalls::timer_settime(
      static_cast<int>(reinterpret_cast<uintptr_t>(timerid)), flags, new_value,
      old_value);
  if (!result) {
    libc_errno = result.error();
    return -1;
  }
  return 0;
}

} // namespace LIBC_NAMESPACE_DECL
