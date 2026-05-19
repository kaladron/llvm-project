//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Unittests for alarm.
///
//===----------------------------------------------------------------------===//

#include "hdr/signal_macros.h"
#include "src/signal/signal.h"
#include "src/unistd/alarm.h"
#include "test/UnitTest/Test.h"

static bool alarm_fired = false;
extern "C" void alarm_handler(int) { alarm_fired = true; }

TEST(LlvmLibcAlarmTest, Basic) {
  LIBC_NAMESPACE::signal(SIGALRM, alarm_handler);
  alarm_fired = false;

  // Set alarm for 10 seconds.
  unsigned int prev = LIBC_NAMESPACE::alarm(10);
  // Initial alarm should be 0.
  EXPECT_EQ(prev, 0U);

  // Set alarm for 5 seconds, should return ~10.
  // We use 0 as a special case if we want to cancel, but here we just re-set.
  prev = LIBC_NAMESPACE::alarm(5);
  EXPECT_GT(prev, 0U);
  EXPECT_LE(prev, 10U);

  // Cancel alarm.
  prev = LIBC_NAMESPACE::alarm(0);
  EXPECT_GT(prev, 0U);
  EXPECT_LE(prev, 5U);

  // Ensure it didn't fire (it shouldn't have in this short time).
  EXPECT_FALSE(alarm_fired);
}
