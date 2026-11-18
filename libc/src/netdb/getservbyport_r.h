//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation header for getservbyport_r.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC_NETDB_GETSERVBYPORT_R_H
#define LLVM_LIBC_SRC_NETDB_GETSERVBYPORT_R_H

#include "hdr/types/size_t.h"
#include "hdr/types/struct_servent.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

int getservbyport_r(int port, const char *__restrict proto,
                    struct servent *__restrict result_buf, char *__restrict buf,
                    size_t buflen, struct servent **__restrict result);

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_NETDB_GETSERVBYPORT_R_H
