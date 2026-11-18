//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of getnetbyname_r.
///
//===----------------------------------------------------------------------===//

#include "src/netdb/getnetbyname_r.h"

#include "hdr/types/size_t.h"
#include "hdr/types/struct_netent.h"
#include "src/__support/CPP/span.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/common.h"
#include "src/__support/macros/null_check.h"
#include "src/netdb/getnet_internal.h"

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(int, getnetbyname_r,
                   (const char *__restrict name,
                    struct netent *__restrict result_buf, char *__restrict buf,
                    size_t buflen, struct netent **__restrict result,
                    int *__restrict h_errnop)) {
  LIBC_CRASH_ON_NULLPTR(name);
  LIBC_CRASH_ON_NULLPTR(result_buf);
  if (buflen > 0)
    LIBC_CRASH_ON_NULLPTR(buf);
  LIBC_CRASH_ON_NULLPTR(result);
  LIBC_CRASH_ON_NULLPTR(h_errnop);

  return netdb::lookup_net_by_name(name, result_buf, {buf, buflen}, result,
                                   h_errnop, netdb::get_networks_path());
}

} // namespace LIBC_NAMESPACE_DECL
