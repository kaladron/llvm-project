//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of internal thread-local h_errno storage.
///
//===----------------------------------------------------------------------===//

#include "src/__support/netdb/h_errno.h"

#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

namespace {

LIBC_THREAD_LOCAL int thread_h_errno = 0;

} // namespace

int *netdb::h_errno_addr() { return &thread_h_errno; }

} // namespace LIBC_NAMESPACE_DECL
