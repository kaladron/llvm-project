//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Linux implementation of the POSIX timer_delete function.
///
//===----------------------------------------------------------------------===//

#include "src/time/timer_delete.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/timer_delete.h"
#include "src/__support/common.h"
#include "src/__support/libc_errno.h"

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(int, timer_delete, (timer_t timerid)) {
  auto result = linux_syscalls::timer_delete(
      static_cast<int>(reinterpret_cast<uintptr_t>(timerid)));
  if (!result) {
    libc_errno = result.error();
    return -1;
  }
  return 0;
}

} // namespace LIBC_NAMESPACE_DECL
