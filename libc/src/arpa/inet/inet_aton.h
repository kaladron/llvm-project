//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Declaration of the inet_aton function.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC_ARPA_INET_INET_ATON_H
#define LLVM_LIBC_SRC_ARPA_INET_INET_ATON_H

#include "hdr/types/struct_in_addr.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

int inet_aton(const char *cp, struct in_addr *inp);

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_ARPA_INET_INET_ATON_H
