//===-- Implementation header of nftw ---------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC_FTW_NFTW_H
#define LLVM_LIBC_SRC_FTW_NFTW_H

#include "src/__support/macros/config.h"
#include "llvm-libc-types/__nftw_func_t.h"

namespace LIBC_NAMESPACE_DECL {

int nftw(const char *dirpath, __nftw_func_t fn, int noopenfd, int flags);

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_FTW_NFTW_H
