//===-- Tests for ErrnoCheckingTest functionality -------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/__support/libc_errno.h"
#include "test/UnitTest/ErrnoCheckingTest.h"
#include "test/UnitTest/LibcTest.h"
#include "test/UnitTest/Test.h"

using LIBC_NAMESPACE::testing::ErrnoCheckingTest;
using LIBC_NAMESPACE::testing::ErrnoGuard;
using LIBC_NAMESPACE::testing::internal::current_context;
using LIBC_NAMESPACE::testing::internal::RunContext;

namespace {

// Mock fixture that leaves errno as 0.
class MockPassFixture : public ErrnoCheckingTest {
public:
  void Run() override { libc_errno = 0; }
  const char *getName() const override { return "MockPassFixture"; }
};

// Mock fixture that leaves errno non-zero without clearing it.
class MockUnclearedErrnoFixture : public ErrnoCheckingTest {
public:
  void Run() override { libc_errno = 1; }
  const char *getName() const override { return "MockUnclearedErrnoFixture"; }
};

// Mock fixture that overrides OnTearDown() for custom cleanup.
class MockOnTearDownFixture : public ErrnoCheckingTest {
public:
  bool on_tear_down_ran = false;

  void Run() override { libc_errno = 0; }
  const char *getName() const override { return "MockOnTearDownFixture"; }

protected:
  void OnTearDown() override { on_tear_down_ran = true; }
};

// Mock fixture whose OnTearDown() sets errno to non-zero.
class MockOnTearDownSetsErrnoFixture : public ErrnoCheckingTest {
public:
  void Run() override { libc_errno = 0; }
  const char *getName() const override {
    return "MockOnTearDownSetsErrnoFixture";
  }

protected:
  void OnTearDown() override { libc_errno = 42; }
};

} // namespace

TEST(LlvmLibcErrnoCheckingTestTest, DerivedFixturePassesWhenErrnoIsZero) {
  MockPassFixture test;
  RunContext ctx;
  RunContext *old_context = current_context;
  current_context = &ctx;

  test.SetUp();
  test.Run();
  test.TearDown();

  current_context = old_context;
  EXPECT_EQ(ctx.status(), RunContext::RunResult::Pass);
}

TEST(LlvmLibcErrnoCheckingTestTest, UnclearedErrnoInDerivedFixtureFails) {
  MockUnclearedErrnoFixture test;
  RunContext ctx;
  RunContext *old_context = current_context;
  current_context = &ctx;

  test.SetUp();
  test.Run();
  test.TearDown();

  current_context = old_context;
  EXPECT_EQ(ctx.status(), RunContext::RunResult::Fail);
  libc_errno = 0;
}

TEST(LlvmLibcErrnoCheckingTestTest, OnTearDownExecutes) {
  MockOnTearDownFixture test;
  RunContext ctx;
  RunContext *old_context = current_context;
  current_context = &ctx;

  test.SetUp();
  test.Run();
  test.TearDown();

  current_context = old_context;
  EXPECT_TRUE(test.on_tear_down_ran);
  EXPECT_EQ(ctx.status(), RunContext::RunResult::Pass);
}

TEST(LlvmLibcErrnoCheckingTestTest, OnTearDownSettingErrnoFails) {
  MockOnTearDownSetsErrnoFixture test;
  RunContext ctx;
  RunContext *old_context = current_context;
  current_context = &ctx;

  test.SetUp();
  test.Run();
  test.TearDown();

  current_context = old_context;
  EXPECT_EQ(ctx.status(), RunContext::RunResult::Fail);
  libc_errno = 0;
}

TEST(LlvmLibcErrnoCheckingTestTest, ErrnoGuardPassesWhenZero) {
  RunContext ctx;
  RunContext *old_context = current_context;
  current_context = &ctx;
  {
    ErrnoGuard guard;
    libc_errno = 0;
  }
  current_context = old_context;
  EXPECT_EQ(ctx.status(), RunContext::RunResult::Pass);
}

TEST(LlvmLibcErrnoCheckingTestTest, ErrnoGuardFailsWhenUncleared) {
  RunContext ctx;
  RunContext *old_context = current_context;
  current_context = &ctx;
  {
    ErrnoGuard guard;
    libc_errno = 1;
  }
  current_context = old_context;
  EXPECT_EQ(ctx.status(), RunContext::RunResult::Fail);
  libc_errno = 0;
}

TEST(LlvmLibcErrnoCheckingTestTest, ErrnoGuardCancelSuppressesFailure) {
  RunContext ctx;
  RunContext *old_context = current_context;
  current_context = &ctx;
  {
    ErrnoGuard guard;
    libc_errno = 1;
    guard.cancel();
  }
  current_context = old_context;
  EXPECT_EQ(ctx.status(), RunContext::RunResult::Pass);
  libc_errno = 0;
}

TEST(LlvmLibcErrnoCheckingTestTest, ErrnoGuardExplicitNoClear) {
  libc_errno = 99;
  RunContext ctx;
  RunContext *old_context = current_context;
  current_context = &ctx;
  {
    ErrnoGuard guard(false);
    // guard(false) should not clear errno upon entry, so errno is still 99.
    EXPECT_EQ(static_cast<int>(libc_errno), 99);
    libc_errno = 0;
  }
  current_context = old_context;
  EXPECT_EQ(ctx.status(), RunContext::RunResult::Pass);
}
