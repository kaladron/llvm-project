//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation header for __h_errno_location.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC_NETDB___H_ERRNO_LOCATION_H
#define LLVM_LIBC_SRC_NETDB___H_ERRNO_LOCATION_H

#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

int *__h_errno_location();

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_NETDB___H_ERRNO_LOCATION_H
