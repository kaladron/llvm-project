//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of getprotobynumber.
///
//===----------------------------------------------------------------------===//

#include "src/netdb/getprotobynumber.h"

#include "hdr/types/size_t.h"
#include "hdr/types/struct_protoent.h"
#include "src/__support/CPP/span.h"
#include "src/__support/common.h"
#include "src/__support/macros/attributes.h"
#include "src/netdb/getproto_internal.h"

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(struct protoent *, getprotobynumber, (int proto)) {
  constexpr size_t PROTO_BUF_SIZE = 1024;
  static LIBC_THREAD_LOCAL struct protoent entry;
  static LIBC_THREAD_LOCAL char buffer[PROTO_BUF_SIZE];

  struct protoent *result = nullptr;
  netdb::lookup_proto_by_num(proto, &entry, buffer, &result,
                             netdb::get_protocols_path());
  return result;
}

} // namespace LIBC_NAMESPACE_DECL
