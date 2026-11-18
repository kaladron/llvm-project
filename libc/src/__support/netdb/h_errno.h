//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Internal thread-local h_errno storage accessor.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_NETDB_H_ERRNO_H
#define LLVM_LIBC_SRC___SUPPORT_NETDB_H_ERRNO_H

#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace netdb {

int *h_errno_addr();

} // namespace netdb
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_NETDB_H_ERRNO_H
