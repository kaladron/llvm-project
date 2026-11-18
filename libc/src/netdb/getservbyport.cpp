//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of getservbyport.
///
//===----------------------------------------------------------------------===//

#include "src/netdb/getservbyport.h"

#include "hdr/types/size_t.h"
#include "hdr/types/struct_servent.h"
#include "src/__support/CPP/span.h"
#include "src/__support/common.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/netdb/services.h"

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(struct servent *, getservbyport,
                   (int port, const char *proto)) {
  constexpr size_t SERV_BUF_SIZE = 1024;
  static LIBC_THREAD_LOCAL struct servent entry;
  static LIBC_THREAD_LOCAL char buffer[SERV_BUF_SIZE];

  struct servent *result = nullptr;
  netdb::lookup_serv_by_port(port, proto, &entry, buffer, &result,
                             netdb::get_services_path());
  return result;
}

} // namespace LIBC_NAMESPACE_DECL
