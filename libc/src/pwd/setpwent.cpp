//===-- Implementation of setpwent ----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/pwd/setpwent.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

// Defined in getpwent.cpp
extern void setpwent_impl();

void setpwent() {
  setpwent_impl();
}

} // namespace LIBC_NAMESPACE_DECL
