//===-- Unittests for localtime -------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/time/localtime.h"
#include "test/UnitTest/Test.h"

// Declare system functions for manipulating environment variables
extern "C" {
int setenv(const char *name, const char *value, int overwrite);
int unsetenv(const char *name);
}

TEST(LlvmLibcLocaltime, ValidUnixTimestamp0) {
  const time_t timer = 0;
  struct tm *result = LIBC_NAMESPACE::localtime(&timer);

  ASSERT_EQ(70, result->tm_year);
  ASSERT_EQ(0, result->tm_mon);
  ASSERT_EQ(1, result->tm_mday);
  ASSERT_EQ(0, result->tm_hour);
  ASSERT_EQ(0, result->tm_min);
  ASSERT_EQ(0, result->tm_sec);
  ASSERT_EQ(4, result->tm_wday);
  ASSERT_EQ(0, result->tm_yday);
  ASSERT_EQ(0, result->tm_isdst);
}

TEST(LlvmLibcLocaltime, NullPtr) {
  EXPECT_DEATH([] { LIBC_NAMESPACE::localtime(nullptr); }, WITH_SIGNAL(4));
}

// TODO(zimirza): These tests does not expect the correct output of localtime as
// per specification. This is due to timezone functions removed from
// https://github.com/llvm/llvm-project/pull/110363.
// This will be resolved a new pull request.

TEST(LlvmLibcLocaltime, ValidUnixTimestamp) {
  const time_t timer = 1756595338;
  struct tm *result = LIBC_NAMESPACE::localtime(&timer);

  ASSERT_EQ(125, result->tm_year);
  ASSERT_EQ(7, result->tm_mon);
  ASSERT_EQ(30, result->tm_mday);
  ASSERT_EQ(23, result->tm_hour);
  ASSERT_EQ(8, result->tm_min);
  ASSERT_EQ(58, result->tm_sec);
  ASSERT_EQ(6, result->tm_wday);
  ASSERT_EQ(241, result->tm_yday);
  ASSERT_EQ(0, result->tm_isdst);
}

TEST(LlvmLibcLocaltime, ValidUnixTimestampNegative) {
  const time_t timer = -1756595338;
  struct tm *result = LIBC_NAMESPACE::localtime(&timer);

  ASSERT_EQ(14, result->tm_year);
  ASSERT_EQ(4, result->tm_mon);
  ASSERT_EQ(4, result->tm_mday);
  ASSERT_EQ(0, result->tm_hour);
  ASSERT_EQ(51, result->tm_min);
  ASSERT_EQ(2, result->tm_sec);
  ASSERT_EQ(1, result->tm_wday);
  ASSERT_EQ(123, result->tm_yday);
  ASSERT_EQ(0, result->tm_isdst);
}

// Tests for TZ environment variable integration
// NOTE: These tests use setenv/unsetenv from system libc. They will fail to
// link in hermetic test mode. See plan.md Step 6.9 for future work to
// implement setenv/unsetenv in LLVM libc.

TEST(LlvmLibcLocaltime, WithTZ_UTC) {
  // Set TZ to UTC
  setenv("TZ", "UTC0", 1);

  // Epoch time (1970-01-01 00:00:00 UTC)
  const time_t timer = 0;
  struct tm *result = LIBC_NAMESPACE::localtime(&timer);

  // Should be identical to UTC
  ASSERT_EQ(70, result->tm_year);
  ASSERT_EQ(0, result->tm_mon);
  ASSERT_EQ(1, result->tm_mday);
  ASSERT_EQ(0, result->tm_hour);
  ASSERT_EQ(0, result->tm_min);
  ASSERT_EQ(0, result->tm_sec);
  ASSERT_EQ(4, result->tm_wday);
  ASSERT_EQ(0, result->tm_yday);
  ASSERT_EQ(0, result->tm_isdst);

  unsetenv("TZ");
}

TEST(LlvmLibcLocaltime, WithTZ_EST) {
  // Set TZ to EST (UTC-5, no DST)
  setenv("TZ", "EST5", 1);

  // Epoch time (1970-01-01 00:00:00 UTC = 1969-12-31 19:00:00 EST)
  const time_t timer = 0;
  struct tm *result = LIBC_NAMESPACE::localtime(&timer);

  ASSERT_EQ(69, result->tm_year);   // 1969
  ASSERT_EQ(11, result->tm_mon);    // December (0-indexed)
  ASSERT_EQ(31, result->tm_mday);   // 31st
  ASSERT_EQ(19, result->tm_hour);   // 19:00 (7 PM)
  ASSERT_EQ(0, result->tm_min);
  ASSERT_EQ(0, result->tm_sec);
  ASSERT_EQ(3, result->tm_wday);    // Wednesday
  ASSERT_EQ(364, result->tm_yday);  // Day 364 (0-indexed)
  ASSERT_EQ(0, result->tm_isdst);   // No DST

  unsetenv("TZ");
}

TEST(LlvmLibcLocaltime, WithTZ_PST) {
  // Set TZ to PST (UTC-8, no DST)
  setenv("TZ", "PST8", 1);

  // Epoch time (1970-01-01 00:00:00 UTC = 1969-12-31 16:00:00 PST)
  const time_t timer = 0;
  struct tm *result = LIBC_NAMESPACE::localtime(&timer);

  ASSERT_EQ(69, result->tm_year);   // 1969
  ASSERT_EQ(11, result->tm_mon);    // December
  ASSERT_EQ(31, result->tm_mday);   // 31st
  ASSERT_EQ(16, result->tm_hour);   // 16:00 (4 PM)
  ASSERT_EQ(0, result->tm_min);
  ASSERT_EQ(0, result->tm_sec);
  ASSERT_EQ(3, result->tm_wday);    // Wednesday
  ASSERT_EQ(364, result->tm_yday);
  ASSERT_EQ(0, result->tm_isdst);

  unsetenv("TZ");
}

TEST(LlvmLibcLocaltime, WithTZ_EasternHemisphere) {
  // Set TZ to IST (India Standard Time, UTC+5:30)
  setenv("TZ", "IST-5:30", 1);

  // Epoch time (1970-01-01 00:00:00 UTC = 1970-01-01 05:30:00 IST)
  const time_t timer = 0;
  struct tm *result = LIBC_NAMESPACE::localtime(&timer);

  ASSERT_EQ(70, result->tm_year);   // 1970
  ASSERT_EQ(0, result->tm_mon);     // January
  ASSERT_EQ(1, result->tm_mday);    // 1st
  ASSERT_EQ(5, result->tm_hour);    // 05:30
  ASSERT_EQ(30, result->tm_min);
  ASSERT_EQ(0, result->tm_sec);
  ASSERT_EQ(4, result->tm_wday);    // Thursday
  ASSERT_EQ(0, result->tm_yday);
  ASSERT_EQ(0, result->tm_isdst);

  unsetenv("TZ");
}

TEST(LlvmLibcLocaltime, WithTZ_DST_StandardTime) {
  // Set TZ to US Eastern time with DST
  setenv("TZ", "EST5EDT,M3.2.0,M11.1.0", 1);

  // January 15, 2024 12:00:00 UTC (during standard time)
  // Expected: January 15, 2024 07:00:00 EST
  const time_t timer = 1705320000; // 2024-01-15 12:00:00 UTC
  struct tm *result = LIBC_NAMESPACE::localtime(&timer);

  ASSERT_EQ(124, result->tm_year);  // 2024
  ASSERT_EQ(0, result->tm_mon);     // January
  ASSERT_EQ(15, result->tm_mday);   // 15th
  ASSERT_EQ(7, result->tm_hour);    // 07:00 (12:00 UTC - 5 hours)
  ASSERT_EQ(0, result->tm_min);
  ASSERT_EQ(0, result->tm_sec);
  ASSERT_EQ(0, result->tm_isdst);   // Standard time (not DST)

  unsetenv("TZ");
}

TEST(LlvmLibcLocaltime, WithTZ_DST_DaylightTime) {
  // Set TZ to US Eastern time with DST
  setenv("TZ", "EST5EDT,M3.2.0,M11.1.0", 1);

  // July 15, 2024 12:00:00 UTC (during daylight saving time)
  // Expected: July 15, 2024 08:00:00 EDT
  const time_t timer = 1721044800; // 2024-07-15 12:00:00 UTC
  struct tm *result = LIBC_NAMESPACE::localtime(&timer);

  ASSERT_EQ(124, result->tm_year);  // 2024
  ASSERT_EQ(6, result->tm_mon);     // July
  ASSERT_EQ(15, result->tm_mday);   // 15th
  ASSERT_EQ(8, result->tm_hour);    // 08:00 (12:00 UTC - 4 hours for EDT)
  ASSERT_EQ(0, result->tm_min);
  ASSERT_EQ(0, result->tm_sec);
  ASSERT_EQ(1, result->tm_isdst);   // Daylight saving time

  unsetenv("TZ");
}

TEST(LlvmLibcLocaltime, WithTZ_InvalidSpec) {
  // Set TZ to an invalid spec (should fall back to UTC)
  setenv("TZ", "INVALID_TIMEZONE_SPEC", 1);

  // Epoch time (1970-01-01 00:00:00 UTC)
  const time_t timer = 0;
  struct tm *result = LIBC_NAMESPACE::localtime(&timer);

  // Should behave like UTC since the TZ spec is invalid
  ASSERT_EQ(70, result->tm_year);
  ASSERT_EQ(0, result->tm_mon);
  ASSERT_EQ(1, result->tm_mday);
  ASSERT_EQ(0, result->tm_hour);
  ASSERT_EQ(0, result->tm_min);
  ASSERT_EQ(0, result->tm_sec);

  unsetenv("TZ");
}

TEST(LlvmLibcLocaltime, WithTZ_EmptyString) {
  // Set TZ to empty string (should use UTC)
  setenv("TZ", "", 1);

  // Epoch time (1970-01-01 00:00:00 UTC)
  const time_t timer = 0;
  struct tm *result = LIBC_NAMESPACE::localtime(&timer);

  // Should behave like UTC
  ASSERT_EQ(70, result->tm_year);
  ASSERT_EQ(0, result->tm_mon);
  ASSERT_EQ(1, result->tm_mday);
  ASSERT_EQ(0, result->tm_hour);
  ASSERT_EQ(0, result->tm_min);
  ASSERT_EQ(0, result->tm_sec);

  unsetenv("TZ");
}

TEST(LlvmLibcLocaltime, WithoutTZ_DefaultsToUTC) {
  // Ensure TZ is not set
  unsetenv("TZ");

  // Epoch time (1970-01-01 00:00:00 UTC)
  const time_t timer = 0;
  struct tm *result = LIBC_NAMESPACE::localtime(&timer);

  // Should behave like UTC when TZ is not set
  ASSERT_EQ(70, result->tm_year);
  ASSERT_EQ(0, result->tm_mon);
  ASSERT_EQ(1, result->tm_mday);
  ASSERT_EQ(0, result->tm_hour);
  ASSERT_EQ(0, result->tm_min);
  ASSERT_EQ(0, result->tm_sec);
}
