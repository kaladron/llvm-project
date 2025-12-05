//===-- Definition of regoff_t type ---------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_REGOFF_T_H
#define LLVM_LIBC_TYPES_REGOFF_T_H

// POSIX requires regoff_t to be a signed integer type capable of holding
// the largest value that can be stored in either a ptrdiff_t or ssize_t type.
typedef __PTRDIFF_TYPE__ regoff_t;

#endif // LLVM_LIBC_TYPES_REGOFF_T_H
