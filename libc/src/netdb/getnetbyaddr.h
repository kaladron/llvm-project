//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation header for getnetbyaddr.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC_NETDB_GETNETBYADDR_H
#define LLVM_LIBC_SRC_NETDB_GETNETBYADDR_H

#include "hdr/stdint_proxy.h"
#include "hdr/types/struct_netent.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

struct netent *getnetbyaddr(uint32_t net, int type);

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_NETDB_GETNETBYADDR_H
