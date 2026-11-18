//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation header for getservbyname.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC_NETDB_GETSERVBYNAME_H
#define LLVM_LIBC_SRC_NETDB_GETSERVBYNAME_H

#include "hdr/types/struct_servent.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

struct servent *getservbyname(const char *name, const char *proto);

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_NETDB_GETSERVBYNAME_H
