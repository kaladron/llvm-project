//===-- Implementation of ftw function ------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/ftw/ftw.h"
#include "src/ftw/ftw_impl.h"

#include "src/__support/common.h"

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(int, ftw,
                   (const char *dirPath, __ftw_func_t fn, int fdLimit)) {
  ftw_impl::CallbackWrapper wrapper;
  wrapper.is_nftw = false;
  wrapper.ftw_fn = fn;
  return ftw_impl::doMergedFtw(dirPath, wrapper, fdLimit, FTW_PHYS, 0);
}

} // namespace LIBC_NAMESPACE_DECL
