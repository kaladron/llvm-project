//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation header for getnetbyname_r.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC_NETDB_GETNETBYNAME_R_H
#define LLVM_LIBC_SRC_NETDB_GETNETBYNAME_R_H

#include "hdr/types/size_t.h"
#include "hdr/types/struct_netent.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

int getnetbyname_r(const char *__restrict name,
                   struct netent *__restrict result_buf, char *__restrict buf,
                   size_t buflen, struct netent **__restrict result,
                   int *__restrict h_errnop);

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_NETDB_GETNETBYNAME_R_H
