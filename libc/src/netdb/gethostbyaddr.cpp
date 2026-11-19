//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of gethostbyaddr.
///
//===----------------------------------------------------------------------===//

#include "src/netdb/gethostbyaddr.h"

#include "hdr/types/size_t.h"
#include "hdr/types/socklen_t.h"
#include "hdr/types/struct_hostent.h"
#include "src/__support/CPP/span.h"
#include "src/__support/common.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/null_check.h"
#include "src/__support/netdb/h_errno.h"
#include "src/__support/netdb/hostent.h"
#include "src/__support/netdb/hosts.h"

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(struct hostent *, gethostbyaddr,
                   (const void *addr, socklen_t len, int type)) {
  LIBC_CRASH_ON_NULLPTR(addr);

  constexpr size_t HOST_BUF_SIZE = 1024;
  static LIBC_THREAD_LOCAL struct hostent entry;
  static LIBC_THREAD_LOCAL char buffer[HOST_BUF_SIZE];

  struct hostent *result = nullptr;
  int herr = 0;
  netdb::lookup_host_byaddr(addr, len, type, &entry, buffer, &result, &herr,
                            netdb::get_hosts_path());
  if (result == nullptr)
    netdb::set_h_errno(herr);
  return result;
}

} // namespace LIBC_NAMESPACE_DECL
