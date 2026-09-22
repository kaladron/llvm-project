//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Unittests for capget.
///
//===----------------------------------------------------------------------===//

#include "hdr/errno_macros.h"
#include "src/sys/capability/capget.h"
#include "test/UnitTest/ErrnoCheckingTest.h"
#include "test/UnitTest/ErrnoSetterMatcher.h"
#include "test/UnitTest/Test.h"
#include <linux/capability.h>

using LIBC_NAMESPACE::testing::ErrnoSetterMatcher::Fails;
using LIBC_NAMESPACE::testing::ErrnoSetterMatcher::Succeeds;
using LlvmLibcCapgetTest = LIBC_NAMESPACE::testing::ErrnoCheckingTest;

TEST_F(LlvmLibcCapgetTest, QueryCurrentProcessV3) {
  __user_cap_header_struct hdr = {_LINUX_CAPABILITY_VERSION_3, 0};
  __user_cap_data_struct data[_LINUX_CAPABILITY_U32S_3] = {};

  ASSERT_THAT(LIBC_NAMESPACE::capget(&hdr, data), Succeeds(0));
  EXPECT_EQ(hdr.version,
            static_cast<unsigned int>(_LINUX_CAPABILITY_VERSION_3));
}

TEST_F(LlvmLibcCapgetTest, ProbePreferredVersionWithNullData) {
  __user_cap_header_struct hdr = {0, 0};

  ASSERT_THAT(LIBC_NAMESPACE::capget(&hdr, nullptr), Succeeds(0));
  EXPECT_NE(hdr.version, 0U);
}

TEST_F(LlvmLibcCapgetTest, InvalidVersionWithNonNullDataFails) {
  __user_cap_header_struct hdr = {0, 0};
  __user_cap_data_struct data[_LINUX_CAPABILITY_U32S_3] = {};

  EXPECT_THAT(LIBC_NAMESPACE::capget(&hdr, data), Fails(EINVAL));
  EXPECT_NE(hdr.version, 0U);
}

TEST_F(LlvmLibcCapgetTest, InvalidPidFails) {
  __user_cap_header_struct hdr = {_LINUX_CAPABILITY_VERSION_3, -1};
  __user_cap_data_struct data[_LINUX_CAPABILITY_U32S_3] = {};

  EXPECT_THAT(LIBC_NAMESPACE::capget(&hdr, data), Fails(EINVAL));
}

TEST_F(LlvmLibcCapgetTest, NullHeaderFails) {
  __user_cap_data_struct data[_LINUX_CAPABILITY_U32S_3] = {};

  EXPECT_THAT(LIBC_NAMESPACE::capget(nullptr, data), Fails(EFAULT));
}
