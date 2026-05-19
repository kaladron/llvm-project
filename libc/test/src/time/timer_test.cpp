//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Unittests for timer_create, timer_settime, timer_gettime and timer_delete.
///
//===----------------------------------------------------------------------===//

#include "hdr/time_macros.h"
#include "hdr/types/clockid_t.h"
#include "hdr/types/struct_itimerspec.h"
#include "hdr/types/struct_sigevent.h"
#include "hdr/types/timer_t.h"
#include "src/signal/sigaction.h"
#include "src/signal/sigemptyset.h"
#include "src/time/timer_create.h"
#include "src/time/timer_delete.h"
#include "src/time/timer_getoverrun.h"
#include "src/time/timer_gettime.h"
#include "src/time/timer_settime.h"
#include "test/UnitTest/ErrnoSetterMatcher.h"
#include "test/UnitTest/Test.h"

#include "hdr/signal_macros.h"

using namespace LIBC_NAMESPACE::testing::ErrnoSetterMatcher;

static bool timer_fired(false);
extern "C" void handler(int) { timer_fired = true; }

TEST(LlvmLibcTimerTest, CreateSetGetDelete) {
  timer_t timerid;
  struct sigevent sev;
  sev.sigev_notify = SIGEV_NONE;

  ASSERT_THAT(LIBC_NAMESPACE::timer_create(CLOCK_REALTIME, &sev, &timerid),
              returns(EQ(0)).with_errno(EQ(0)));

  struct itimerspec its;
  its.it_value.tv_sec = 10;
  its.it_value.tv_nsec = 0;
  its.it_interval.tv_sec = 0;
  its.it_interval.tv_nsec = 0;

  ASSERT_THAT(LIBC_NAMESPACE::timer_settime(timerid, 0, &its, nullptr),
              returns(EQ(0)).with_errno(EQ(0)));

  struct itimerspec curr_its;
  ASSERT_THAT(LIBC_NAMESPACE::timer_gettime(timerid, &curr_its),
              returns(EQ(0)).with_errno(EQ(0)));

  ASSERT_LE(curr_its.it_value.tv_sec, time_t(10));

  ASSERT_THAT(LIBC_NAMESPACE::timer_getoverrun(timerid),
              returns(GE(0)).with_errno(EQ(0)));

  ASSERT_THAT(LIBC_NAMESPACE::timer_delete(timerid),
              returns(EQ(0)).with_errno(EQ(0)));
}

TEST(LlvmLibcTimerTest, InvalidTimerDelete) {
  // Use a likely invalid timer ID
  timer_t invalid_timerid = reinterpret_cast<timer_t>(-1);
  ASSERT_THAT(LIBC_NAMESPACE::timer_delete(invalid_timerid),
              returns(EQ(-1)).with_errno(NE(0)));
}

TEST(LlvmLibcTimerTest, Signal) {
  timer_fired = false;
  struct sigaction sa;
  sa.sa_handler = handler;
  LIBC_NAMESPACE::sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  ASSERT_THAT(LIBC_NAMESPACE::sigaction(SIGALRM, &sa, nullptr),
              returns(EQ(0)).with_errno(EQ(0)));

  timer_t timerid;
  struct sigevent sev;
  sev.sigev_notify = SIGEV_SIGNAL;
  sev.sigev_signo = SIGALRM;

  ASSERT_THAT(LIBC_NAMESPACE::timer_create(CLOCK_REALTIME, &sev, &timerid),
              returns(EQ(0)).with_errno(EQ(0)));

  struct itimerspec its;
  its.it_value.tv_sec = 0;
  its.it_value.tv_nsec = 100000000; // 100ms
  its.it_interval.tv_sec = 0;
  its.it_interval.tv_nsec = 0;

  ASSERT_THAT(LIBC_NAMESPACE::timer_settime(timerid, 0, &its, nullptr),
              returns(EQ(0)).with_errno(EQ(0)));

  while (!timer_fired) {
    // Wait for timer
  }

  ASSERT_TRUE(timer_fired);
  ASSERT_THAT(LIBC_NAMESPACE::timer_delete(timerid),
              returns(EQ(0)).with_errno(EQ(0)));
}
