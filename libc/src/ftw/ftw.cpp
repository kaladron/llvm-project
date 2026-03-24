//===-- Implementation of ftw function ------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "hdr/ftw_macros.h"
#include "src/ftw/ftw.h"
#include "src/ftw/ftw_impl.h"

#include "src/__support/common.h"
#include "src/__support/libc_errno.h"

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(int, ftw,
                   (const char *dirPath, __ftw_func_t fn, int fdLimit)) {
  ftw_impl::CallbackWrapper wrapper;
  wrapper.isNftw = false;
  wrapper.ftwFn = fn;
  auto result = ftw_impl::doMergedFtw(dirPath, wrapper, fdLimit, 0, 0, 0, nullptr);
  if (!result) {
    libc_errno = result.error();
    return -1;
  }
  return result.value();
}

} // namespace LIBC_NAMESPACE_DECL
