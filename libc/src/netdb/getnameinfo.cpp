//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of getnameinfo.
///
//===----------------------------------------------------------------------===//

#include "src/netdb/getnameinfo.h"

#include "hdr/netdb_macros.h"
#include "hdr/types/socklen_t.h"
#include "hdr/types/struct_sockaddr.h"
#include "src/__support/common.h"
#include "src/__support/libc_errno.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/null_check.h"
#include "src/__support/netdb/nameinfo.h"

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(int, getnameinfo,
                   (const struct sockaddr *__restrict sa, socklen_t salen,
                    char *__restrict node, socklen_t nodelen,
                    char *__restrict service, socklen_t servicelen,
                    int flags)) {
  LIBC_CRASH_ON_NULLPTR(sa);

  int sys_err = 0;
  const int RC = netdb::lookup_nameinfo(sa, salen, node, nodelen, service,
                                        servicelen, flags, sys_err);
  if (RC == EAI_SYSTEM)
    libc_errno = sys_err;
  return RC;
}

} // namespace LIBC_NAMESPACE_DECL
