//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation header for gethostbyaddr.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC_NETDB_GETHOSTBYADDR_H
#define LLVM_LIBC_SRC_NETDB_GETHOSTBYADDR_H

#include "hdr/types/socklen_t.h"
#include "hdr/types/struct_hostent.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

struct hostent *gethostbyaddr(const void *addr, socklen_t len, int type);

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_NETDB_GETHOSTBYADDR_H
