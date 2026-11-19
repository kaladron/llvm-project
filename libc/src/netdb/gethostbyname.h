//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation header for gethostbyname.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC_NETDB_GETHOSTBYNAME_H
#define LLVM_LIBC_SRC_NETDB_GETHOSTBYNAME_H

#include "hdr/types/struct_hostent.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

struct hostent *gethostbyname(const char *name);

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_NETDB_GETHOSTBYNAME_H
