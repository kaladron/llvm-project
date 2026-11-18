//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of getnetbyname.
///
//===----------------------------------------------------------------------===//

#include "src/netdb/getnetbyname.h"

#include "hdr/types/size_t.h"
#include "hdr/types/struct_netent.h"
#include "src/__support/CPP/span.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/common.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/null_check.h"
#include "src/netdb/getnet_internal.h"

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(struct netent *, getnetbyname, (const char *name)) {
  LIBC_CRASH_ON_NULLPTR(name);

  constexpr size_t NET_BUF_SIZE = 1024;
  static LIBC_THREAD_LOCAL struct netent entry;
  static LIBC_THREAD_LOCAL char buffer[NET_BUF_SIZE];

  struct netent *result = nullptr;
  int herr = 0;
  netdb::lookup_net_by_name(name, &entry, buffer, &result, &herr,
                            netdb::get_networks_path());
  return result;
}

} // namespace LIBC_NAMESPACE_DECL
