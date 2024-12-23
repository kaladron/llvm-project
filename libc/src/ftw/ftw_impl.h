//===-- Internal implementation for ftw/nftw ------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC_FTW_FTW_IMPL_H
#define LLVM_LIBC_SRC_FTW_FTW_IMPL_H

#include "src/__support/CPP/string.h"
#include "src/__support/macros/config.h"

#include <ftw.h>
#include <sys/stat.h>

namespace LIBC_NAMESPACE_DECL {
namespace ftw_impl {

using nftwFn = int (*)(const char *filePath, const struct stat *statBuf,
                       int tFlag, struct FTW *ftwbuf);

using ftwFn = int (*)(const char *filePath, const struct stat *statBuf,
                      int tFlag);

// Unified callback wrapper - uses a union to avoid virtual functions
struct CallbackWrapper {
  bool is_nftw;
  union {
    nftwFn nftw_fn;
    ftwFn ftw_fn;
  };

  LIBC_INLINE int call(const char *path, const struct stat *sb, int type,
                       struct FTW *ftwbuf) const {
    if (is_nftw)
      return nftw_fn(path, sb, type, ftwbuf);
    else
      return ftw_fn(path, sb, type);
  }
};

// Main implementation function - defined in ftw_impl.cpp
int doMergedFtw(const cpp::string &dirPath, const CallbackWrapper &fn,
                int fdLimit, int flags, int level);

} // namespace ftw_impl
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_FTW_FTW_IMPL_H
