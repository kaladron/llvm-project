//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Unittests for capset.
///
//===----------------------------------------------------------------------===//

#include "hdr/errno_macros.h"
#include "src/sys/capability/capget.h"
#include "src/sys/capability/capset.h"
#include "test/UnitTest/ErrnoCheckingTest.h"
#include "test/UnitTest/ErrnoSetterMatcher.h"
#include "test/UnitTest/Test.h"
#include <linux/capability.h>

using LIBC_NAMESPACE::testing::ErrnoSetterMatcher::Fails;
using LIBC_NAMESPACE::testing::ErrnoSetterMatcher::Succeeds;
using LlvmLibcCapsetTest = LIBC_NAMESPACE::testing::ErrnoCheckingTest;

TEST_F(LlvmLibcCapsetTest, SetUnchangedCurrentCapabilities) {
  __user_cap_header_struct hdr = {_LINUX_CAPABILITY_VERSION_3, 0};
  __user_cap_data_struct data[_LINUX_CAPABILITY_U32S_3] = {};

  ASSERT_THAT(LIBC_NAMESPACE::capget(&hdr, data), Succeeds(0));
  EXPECT_THAT(LIBC_NAMESPACE::capset(&hdr, data), Succeeds(0));
}

TEST_F(LlvmLibcCapsetTest, InvalidVersionFails) {
  __user_cap_header_struct hdr = {0, 0};
  __user_cap_data_struct data[_LINUX_CAPABILITY_U32S_3] = {};

  EXPECT_THAT(LIBC_NAMESPACE::capset(&hdr, data), Fails(EINVAL));
  EXPECT_NE(hdr.version, 0U);
}

TEST_F(LlvmLibcCapsetTest, NullPointersFail) {
  __user_cap_header_struct hdr = {_LINUX_CAPABILITY_VERSION_3, 0};
  __user_cap_data_struct data[_LINUX_CAPABILITY_U32S_3] = {};

  EXPECT_THAT(LIBC_NAMESPACE::capset(nullptr, data), Fails(EFAULT));
  EXPECT_THAT(LIBC_NAMESPACE::capset(&hdr, nullptr), Fails(EFAULT));
}
