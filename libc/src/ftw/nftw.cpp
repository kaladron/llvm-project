//===-- Implementation of nftw function -----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/ftw/nftw.h"
#include "src/ftw/ftw_impl.h"

#include "src/__support/common.h"

#include <ftw.h>

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(int, nftw,
                   (const char *dirPath, __nftw_func_t fn, int fdLimit,
                    int flags)) {
  ftw_impl::CallbackWrapper wrapper;
  wrapper.is_nftw = true;
  wrapper.nftw_fn = fn;
  return ftw_impl::doMergedFtw(dirPath, wrapper, fdLimit, flags, 0);
}

} // namespace LIBC_NAMESPACE_DECL
