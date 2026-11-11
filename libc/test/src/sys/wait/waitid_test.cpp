//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Unittests for waitid.
///
//===----------------------------------------------------------------------===//

#include "hdr/errno_macros.h"
#include "hdr/sys_wait_macros.h"
#include "hdr/types/id_t.h"
#include "hdr/types/idtype_t.h"
#include "hdr/types/siginfo_t.h"
#include "src/sys/wait/waitid.h"
#include "test/UnitTest/ErrnoCheckingTest.h"
#include "test/UnitTest/ErrnoSetterMatcher.h"
#include "test/UnitTest/Test.h"

using namespace LIBC_NAMESPACE::testing::ErrnoSetterMatcher;
using LlvmLibcWaitidTest = LIBC_NAMESPACE::testing::ErrnoCheckingTest;

// The test here is a simple test for error handling and WNOHANG functionality.
// For a more involved test, look at fork_test.

TEST_F(LlvmLibcWaitidTest, InvalidFlags) {
  siginfo_t info{};
  // POSIX and Linux require at least one of WEXITED, WSTOPPED, or WCONTINUED.
  ASSERT_THAT(LIBC_NAMESPACE::waitid(P_ALL, 0, &info, 0), Fails(EINVAL));
}

TEST_F(LlvmLibcWaitidTest, InvalidId) {
  siginfo_t info{};
  ASSERT_THAT(
      LIBC_NAMESPACE::waitid(P_PID, static_cast<id_t>(-1), &info, WEXITED),
      Fails(EINVAL));
}

#ifdef LIBC_FULL_BUILD
// In overlay mode, glibc defines idtype_t as an unscoped C++ enum without a
// fixed underlying type, making out-of-range values undefined behaviour in C++.
TEST_F(LlvmLibcWaitidTest, InvalidIdType) {
  siginfo_t info{};
  ASSERT_THAT(
      LIBC_NAMESPACE::waitid(static_cast<idtype_t>(-1), 0, &info, WEXITED),
      Fails(EINVAL));
}
#endif // LIBC_FULL_BUILD

TEST_F(LlvmLibcWaitidTest, NoHangNoChild) {
  siginfo_t info{};
  ASSERT_THAT(LIBC_NAMESPACE::waitid(P_ALL, 0, &info, WEXITED | WNOHANG),
              Fails(ECHILD));
}
