//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Linux implementation of the POSIX timer_create function.
///
//===----------------------------------------------------------------------===//

#include "src/time/timer_create.h"
#include "hdr/types/struct_sigevent.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/timer_create.h"
#include "src/__support/common.h"
#include "src/__support/libc_errno.h"

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(int, timer_create,
                   (clockid_t clock_id, const struct sigevent *__restrict evp,
                    timer_t *__restrict timerid)) {
  int kernel_timer_id;
  auto result = linux_syscalls::timer_create(clock_id, evp, &kernel_timer_id);
  if (!result) {
    libc_errno = result.error();
    return -1;
  }
  *timerid = reinterpret_cast<timer_t>(static_cast<uintptr_t>(kernel_timer_id));
  return 0;
}

} // namespace LIBC_NAMESPACE_DECL
