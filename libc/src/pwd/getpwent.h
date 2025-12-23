//===-- Implementation header for getpwent ----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC_PWD_GETPWENT_H
#define LLVM_LIBC_SRC_PWD_GETPWENT_H

#include "src/__support/macros/config.h"
#include <pwd.h>

namespace LIBC_NAMESPACE_DECL {

struct passwd *getpwent();

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_PWD_GETPWENT_H
