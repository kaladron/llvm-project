//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Linux implementation of capget.
///
//===----------------------------------------------------------------------===//

#include "src/sys/capability/capget.h"

#include "src/__support/OSUtil/linux/syscall_wrappers/capget.h"
#include "src/__support/common.h"
#include "src/__support/libc_errno.h"
#include "src/__support/macros/config.h"
#include <linux/capability.h>

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(int, capget,
                   (cap_user_header_t hdrp, cap_user_data_t datap)) {
  ErrorOr<int> ret = linux_syscalls::capget(hdrp, datap);
  if (!ret) {
    libc_errno = ret.error();
    return -1;
  }
  return ret.value();
}

} // namespace LIBC_NAMESPACE_DECL
