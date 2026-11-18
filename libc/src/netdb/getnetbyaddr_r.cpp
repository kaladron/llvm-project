//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of getnetbyaddr_r.
///
//===----------------------------------------------------------------------===//

#include "src/netdb/getnetbyaddr_r.h"

#include "hdr/stdint_proxy.h"
#include "hdr/types/size_t.h"
#include "hdr/types/struct_netent.h"
#include "src/__support/CPP/span.h"
#include "src/__support/common.h"
#include "src/__support/macros/null_check.h"
#include "src/netdb/getnet_internal.h"

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(int, getnetbyaddr_r,
                   (uint32_t net, int type,
                    struct netent *__restrict result_buf, char *__restrict buf,
                    size_t buflen, struct netent **__restrict result,
                    int *__restrict h_errnop)) {
  LIBC_CRASH_ON_NULLPTR(result_buf);
  if (buflen > 0)
    LIBC_CRASH_ON_NULLPTR(buf);
  LIBC_CRASH_ON_NULLPTR(result);
  LIBC_CRASH_ON_NULLPTR(h_errnop);

  return netdb::lookup_net_by_addr(net, type, result_buf, {buf, buflen}, result,
                                   h_errnop, netdb::get_networks_path());
}

} // namespace LIBC_NAMESPACE_DECL
