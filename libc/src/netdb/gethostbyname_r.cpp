//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of gethostbyname_r.
///
//===----------------------------------------------------------------------===//

#include "src/netdb/gethostbyname_r.h"

#include "hdr/types/size_t.h"
#include "hdr/types/struct_hostent.h"
#include "src/__support/CPP/span.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/common.h"
#include "src/__support/macros/null_check.h"
#include "src/__support/netdb/hostent.h"
#include "src/__support/netdb/hosts.h"

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(int, gethostbyname_r,
                   (const char *__restrict name, struct hostent *__restrict ret,
                    char *__restrict buf, size_t buflen,
                    struct hostent **__restrict result,
                    int *__restrict h_errnop)) {
  LIBC_CRASH_ON_NULLPTR(name);
  LIBC_CRASH_ON_NULLPTR(ret);
  if (buflen > 0)
    LIBC_CRASH_ON_NULLPTR(buf);
  LIBC_CRASH_ON_NULLPTR(result);

  return netdb::lookup_host_byname(name, ret, {buf, buflen}, result, h_errnop,
                                   netdb::get_hosts_path());
}

} // namespace LIBC_NAMESPACE_DECL
