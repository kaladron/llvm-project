//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/pwd/endpwent.h"
#include "src/__support/common.h"
#include "src/__support/libc_errno.h"
#include "src/pwd/pwd_utils.h"

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(void, endpwent, ()) {
  // endpwent_impl closes the password file. If an error occurs,
  // it returns an errno value which is set here.
  int ret = endpwent_impl();
  if (ret != 0)
    libc_errno = ret;
}

} // namespace LIBC_NAMESPACE_DECL
