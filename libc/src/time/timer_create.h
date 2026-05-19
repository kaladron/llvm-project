//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Internal header for timer_create.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC_TIME_TIMER_CREATE_H
#define LLVM_LIBC_SRC_TIME_TIMER_CREATE_H

#include "hdr/types/clockid_t.h"
#include "hdr/types/struct_sigevent.h"
#include "hdr/types/timer_t.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

int timer_create(clockid_t clock_id, const struct sigevent *__restrict evp,
                 timer_t *__restrict timerid);

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_TIME_TIMER_CREATE_H
