//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of struct sigevent.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_SIGEVENT_H
#define LLVM_LIBC_TYPES_STRUCT_SIGEVENT_H

#include "union_sigval.h"

// This definition matches the Linux kernel's definition of struct sigevent.
// See include/uapi/linux/signal.h in the kernel source.

struct sigevent {
  union sigval sigev_value;
  int sigev_signo;
  int sigev_notify;
  union {
    int _pad[64 / sizeof(int) - 4]; // union sigval is 2 ints or 1 long
    int _tid;
    struct {
      void (*_function)(union sigval);
      void *_attribute; // really pthread_attr_t *
    } _sigev_thread;
  } _sigev_un;
};

#define sigev_notify_function _sigev_un._sigev_thread._function
#define sigev_notify_attributes _sigev_un._sigev_thread._attribute
#define sigev_notify_thread_id _sigev_un._tid

#endif // LLVM_LIBC_TYPES_STRUCT_SIGEVENT_H
