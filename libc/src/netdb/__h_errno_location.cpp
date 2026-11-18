//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of __h_errno_location.
///
//===----------------------------------------------------------------------===//

#include "src/netdb/__h_errno_location.h"

#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/netdb/h_errno.h"

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(int *, __h_errno_location, ()) {
  return netdb::h_errno_addr();
}

} // namespace LIBC_NAMESPACE_DECL
