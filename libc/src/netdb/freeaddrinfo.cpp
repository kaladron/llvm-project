//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of freeaddrinfo.
///
//===----------------------------------------------------------------------===//

#include "src/netdb/freeaddrinfo.h"

#include "hdr/types/struct_addrinfo.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/netdb/addrinfo.h"

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(void, freeaddrinfo, (struct addrinfo * ai)) {
  netdb::free_addrinfo_list(ai);
}

} // namespace LIBC_NAMESPACE_DECL
