//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of getaddrinfo.
///
//===----------------------------------------------------------------------===//

#include "src/netdb/getaddrinfo.h"

#include "hdr/types/struct_addrinfo.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/null_check.h"
#include "src/__support/netdb/addrinfo.h"

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(int, getaddrinfo,
                   (const char *__restrict nodename,
                    const char *__restrict servname,
                    const struct addrinfo *__restrict hints,
                    struct addrinfo **__restrict res)) {
  LIBC_CRASH_ON_NULLPTR(res);

  return netdb::lookup_addrinfo(nodename, servname, hints, res);
}

} // namespace LIBC_NAMESPACE_DECL
