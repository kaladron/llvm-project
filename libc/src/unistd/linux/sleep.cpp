//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Linux implementation of sleep.
///
//===----------------------------------------------------------------------===//

#include "src/unistd/sleep.h"
#include "hdr/errno_macros.h"
#include "hdr/types/struct_timespec.h"
#include "hdr/types/time_t.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/nanosleep.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

static_assert(sizeof(time_t) == sizeof(int64_t),
              "32-bit time_t is not supported");

LLVM_LIBC_FUNCTION(unsigned int, sleep, (unsigned int seconds)) {
  if (seconds == 0)
    return 0;

  unsigned int remaining = seconds;
  while (remaining > 0) {
    // Since time_t is 64-bit and remaining is 32-bit, remaining always fits
    // into sleep_time without overflow.
    time_t sleep_time = remaining;

    timespec req = {sleep_time, 0};
    timespec rem = {0, 0};
    auto result = linux_syscalls::nanosleep(&req, &rem);
    if (!result) {
      if (result.error() == EINTR) {
        // If nanosleep was interrupted, calculate the remaining unslept time.
        // tv_nsec is rounded up to the next full second to match POSIX sleep
        // return value requirements.
        unsigned int unslept_this_step =
            static_cast<unsigned int>(rem.tv_sec + (rem.tv_nsec > 0 ? 1 : 0));
        return static_cast<unsigned int>(remaining - sleep_time) +
               unslept_this_step;
      }
      // For other errors (e.g. ENOSYS), assume we didn't sleep this step.
      return remaining;
    }
    remaining -= sleep_time;
  }
  return 0;
}

} // namespace LIBC_NAMESPACE_DECL
