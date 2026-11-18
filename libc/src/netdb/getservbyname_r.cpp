//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of getservbyname_r.
///
//===----------------------------------------------------------------------===//

#include "src/netdb/getservbyname_r.h"

#include "hdr/types/size_t.h"
#include "hdr/types/struct_servent.h"
#include "src/__support/CPP/span.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/common.h"
#include "src/__support/macros/null_check.h"
#include "src/__support/netdb/services.h"

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(int, getservbyname_r,
                   (const char *__restrict name, const char *__restrict proto,
                    struct servent *__restrict result_buf, char *__restrict buf,
                    size_t buflen, struct servent **__restrict result)) {
  LIBC_CRASH_ON_NULLPTR(name);
  LIBC_CRASH_ON_NULLPTR(result_buf);
  if (buflen > 0)
    LIBC_CRASH_ON_NULLPTR(buf);
  LIBC_CRASH_ON_NULLPTR(result);

  return netdb::lookup_serv_by_name(name, proto, result_buf, {buf, buflen},
                                    result, netdb::get_services_path());
}

} // namespace LIBC_NAMESPACE_DECL
