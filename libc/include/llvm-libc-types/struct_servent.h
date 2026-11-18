//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of struct servent.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_SERVENT_H
#define LLVM_LIBC_TYPES_STRUCT_SERVENT_H

struct servent {
  char *s_name;
  char **s_aliases;
  int s_port;
  char *s_proto;
};

#endif // LLVM_LIBC_TYPES_STRUCT_SERVENT_H
