//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of struct netent.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_NETENT_H
#define LLVM_LIBC_TYPES_STRUCT_NETENT_H

struct netent {
  char *n_name;
  char **n_aliases;
  int n_addrtype;
  __UINT32_TYPE__ n_net;
};

#endif // LLVM_LIBC_TYPES_STRUCT_NETENT_H
