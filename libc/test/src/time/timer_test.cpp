//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "hdr/signal_macros.h"
#include "libc/include/llvm-libc-types/struct_sigevent.h"
#include "libc/include/llvm-libc-types/timer_t.h"
#include "src/time/clock_gettime.h"
#include "src/time/timer_create.h"
#include "test/UnitTest/Test.h"

TEST(LlvmLibcTimerTest, CreateTest) {
  timer_t timerid;
  struct sigevent sev;
  sev.sigev_notify = SIGEV_NONE;

  ASSERT_EQ(LIBC_NAMESPACE::timer_create(CLOCK_REALTIME, &sev, &timerid), 0);
}
