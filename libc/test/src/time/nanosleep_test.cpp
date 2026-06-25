//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Unittests for nanosleep.
///
//===----------------------------------------------------------------------===//

#include "hdr/signal_macros.h"
#include "hdr/types/struct_timespec.h"
#include "src/__support/macros/config.h"
#include "src/time/nanosleep.h"
#include "test/UnitTest/ErrnoCheckingTest.h"
#include "test/UnitTest/Test.h"

namespace cpp = LIBC_NAMESPACE::cpp;

using LlvmLibcNanosleep = LIBC_NAMESPACE::testing::ErrnoCheckingTest;

TEST_F(LlvmLibcNanosleep, SmokeTest) {
  // TODO: When we have the code to read clocks, test that time has passed.
  timespec tim = {1, 500};
  timespec tim2 = {0, 0};
  ASSERT_EQ(LIBC_NAMESPACE::nanosleep(&tim, &tim2), 0);
}

TEST_F(LlvmLibcNanosleep, InvalidNsec) {
  timespec tim = {0, -1};
  timespec tim2 = {0, 0};

  libc_errno = 0;
  ASSERT_EQ(LIBC_NAMESPACE::nanosleep(&tim, &tim2), -1);
  ASSERT_ERRNO_EQ(EINVAL);

  tim = {0, 1000000000};
  libc_errno = 0;
  ASSERT_EQ(LIBC_NAMESPACE::nanosleep(&tim, &tim2), -1);
  ASSERT_ERRNO_EQ(EINVAL);
}

TEST_F(LlvmLibcNanosleep, NullPtr) {
  libc_errno = 0;
  ASSERT_EQ(LIBC_NAMESPACE::nanosleep(nullptr, nullptr), -1);
  ASSERT_ERRNO_EQ(EFAULT);
}
