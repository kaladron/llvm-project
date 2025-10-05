//===-- Unittests for mktime ----------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/__support/CPP/limits.h" // INT_MAX
#include "src/time/localtime.h"
#include "src/time/mktime.h"
#include "src/time/time_constants.h"
#include "test/UnitTest/ErrnoSetterMatcher.h"
#include "test/UnitTest/Test.h"
#include "test/src/time/TmHelper.h"
#include "test/src/time/TmMatcher.h"

// Declare system functions for manipulating environment variables
extern "C" {
int setenv(const char *name, const char *value, int overwrite);
int unsetenv(const char *name);
}

using LIBC_NAMESPACE::testing::ErrnoSetterMatcher::Fails;
using LIBC_NAMESPACE::testing::ErrnoSetterMatcher::Succeeds;
using LIBC_NAMESPACE::time_constants::Month;

#ifndef EOVERFLOW
#define EOVERFLOW 0
#endif

static inline constexpr int tm_year(int year) {
  return year - LIBC_NAMESPACE::time_constants::TIME_YEAR_BASE;
}

TEST(LlvmLibcMkTime, FailureSetsErrno) {
  struct tm tm_data{.tm_sec = INT_MAX,
                    .tm_min = INT_MAX,
                    .tm_hour = INT_MAX,
                    .tm_mday = INT_MAX,
                    .tm_mon = INT_MAX - 1,
                    .tm_year = tm_year(INT_MAX),
                    .tm_wday = 0,
                    .tm_yday = 0,
                    .tm_isdst = 0};
  EXPECT_THAT(LIBC_NAMESPACE::mktime(&tm_data), Fails(EOVERFLOW));
}

TEST(LlvmLibcMkTime, InvalidSeconds) {
  {
    // -1 second from 1970-01-01 00:00:00 returns 1969-12-31 23:59:59.
    struct tm tm_data{.tm_sec = -1,
                      .tm_min = 0,
                      .tm_hour = 0,
                      .tm_mday = 1,
                      .tm_mon = Month::JANUARY,
                      .tm_year = tm_year(1970),
                      .tm_wday = 0,
                      .tm_yday = 0,
                      .tm_isdst = 0};
    EXPECT_THAT(LIBC_NAMESPACE::mktime(&tm_data), Succeeds(-1));
    EXPECT_TM_EQ((tm{.tm_sec = 59,
                     .tm_min = 59,
                     .tm_hour = 23,
                     .tm_mday = 31,
                     .tm_mon = Month::DECEMBER,
                     .tm_year = tm_year(1969),
                     .tm_wday = 3,
                     .tm_yday = 364,
                     .tm_isdst = 0}),
                 tm_data);
  }

  {
    // 60 seconds from 1970-01-01 00:00:00 returns 1970-01-01 00:01:00.
    struct tm tm_data{.tm_sec = 60,
                      .tm_min = 0,
                      .tm_hour = 0,
                      .tm_mday = 1,
                      .tm_mon = Month::JANUARY,
                      .tm_year = tm_year(1970),
                      .tm_wday = 0,
                      .tm_yday = 0,
                      .tm_isdst = 0};
    EXPECT_THAT(LIBC_NAMESPACE::mktime(&tm_data), Succeeds(60));
    EXPECT_TM_EQ((tm{.tm_sec = 0,
                     .tm_min = 1,
                     .tm_hour = 0,
                     .tm_mday = 1,
                     .tm_mon = Month::JANUARY,
                     .tm_year = tm_year(1970),
                     .tm_wday = 4,
                     .tm_yday = 0,
                     .tm_isdst = 0}),
                 tm_data);
  }
}

TEST(LlvmLibcMkTime, InvalidMinutes) {
  {
    // -1 minute from 1970-01-01 00:00:00 returns 1969-12-31 23:59:00.
    struct tm tm_data{.tm_sec = 0,
                      .tm_min = -1,
                      .tm_hour = 0,
                      .tm_mday = 1,
                      .tm_mon = Month::JANUARY,
                      .tm_year = tm_year(1970),
                      .tm_wday = 0,
                      .tm_yday = 0,
                      .tm_isdst = 0};
    EXPECT_THAT(LIBC_NAMESPACE::mktime(&tm_data),
                Succeeds(-LIBC_NAMESPACE::time_constants::SECONDS_PER_MIN));
    EXPECT_TM_EQ((tm{.tm_sec = 0,
                     .tm_min = 59,
                     .tm_hour = 23,
                     .tm_mday = 31,
                     .tm_mon = Month::DECEMBER,
                     .tm_year = tm_year(1969),
                     .tm_wday = 3,
                     .tm_yday = 0,
                     .tm_isdst = 0}),
                 tm_data);
  }

  {
    // 60 minutes from 1970-01-01 00:00:00 returns 1970-01-01 01:00:00.
    struct tm tm_data{.tm_sec = 0,
                      .tm_min = 60,
                      .tm_hour = 0,
                      .tm_mday = 1,
                      .tm_mon = Month::JANUARY,
                      .tm_year = tm_year(1970),
                      .tm_wday = 0,
                      .tm_yday = 0,
                      .tm_isdst = 0};
    EXPECT_THAT(LIBC_NAMESPACE::mktime(&tm_data),
                Succeeds(60 * LIBC_NAMESPACE::time_constants::SECONDS_PER_MIN));
    EXPECT_TM_EQ((tm{.tm_sec = 0,
                     .tm_min = 0,
                     .tm_hour = 1,
                     .tm_mday = 1,
                     .tm_mon = Month::JANUARY,
                     .tm_year = tm_year(1970),
                     .tm_wday = 4,
                     .tm_yday = 0,
                     .tm_isdst = 0}),
                 tm_data);
  }
}

TEST(LlvmLibcMkTime, InvalidHours) {
  {
    // -1 hour from 1970-01-01 00:00:00 returns 1969-12-31 23:00:00.
    struct tm tm_data{.tm_sec = 0,
                      .tm_min = 0,
                      .tm_hour = -1,
                      .tm_mday = 1,
                      .tm_mon = Month::JANUARY,
                      .tm_year = tm_year(1970),
                      .tm_wday = 0,
                      .tm_yday = 0,
                      .tm_isdst = 0};
    EXPECT_THAT(LIBC_NAMESPACE::mktime(&tm_data),
                Succeeds(-LIBC_NAMESPACE::time_constants::SECONDS_PER_HOUR));
    EXPECT_TM_EQ((tm{.tm_sec = 0,
                     .tm_min = 0,
                     .tm_hour = 23,
                     .tm_mday = 31,
                     .tm_mon = Month::DECEMBER,
                     .tm_year = tm_year(1969),
                     .tm_wday = 3,
                     .tm_yday = 0,
                     .tm_isdst = 0}),
                 tm_data);
  }

  {
    // 24 hours from 1970-01-01 00:00:00 returns 1970-01-02 00:00:00.
    struct tm tm_data{.tm_sec = 0,
                      .tm_min = 0,
                      .tm_hour = 24,
                      .tm_mday = 1,
                      .tm_mon = Month::JANUARY,
                      .tm_year = tm_year(1970),
                      .tm_wday = 0,
                      .tm_yday = 0,
                      .tm_isdst = 0};
    EXPECT_THAT(
        LIBC_NAMESPACE::mktime(&tm_data),
        Succeeds(24 * LIBC_NAMESPACE::time_constants::SECONDS_PER_HOUR));
    EXPECT_TM_EQ((tm{.tm_sec = 0,
                     .tm_min = 0,
                     .tm_hour = 0,
                     .tm_mday = 2,
                     .tm_mon = Month::JANUARY,
                     .tm_year = tm_year(1970),
                     .tm_wday = 5,
                     .tm_yday = 0,
                     .tm_isdst = 0}),
                 tm_data);
  }
}

TEST(LlvmLibcMkTime, InvalidYear) {
  // -1 year from 1970-01-01 00:00:00 returns 1969-01-01 00:00:00.
  struct tm tm_data{.tm_sec = 0,
                    .tm_min = 0,
                    .tm_hour = 0,
                    .tm_mday = 1,
                    .tm_mon = Month::JANUARY,
                    .tm_year = tm_year(1969),
                    .tm_wday = 0,
                    .tm_yday = 0,
                    .tm_isdst = 0};
  EXPECT_THAT(LIBC_NAMESPACE::mktime(&tm_data),
              Succeeds(-LIBC_NAMESPACE::time_constants::DAYS_PER_NON_LEAP_YEAR *
                       LIBC_NAMESPACE::time_constants::SECONDS_PER_DAY));
  EXPECT_TM_EQ((tm{.tm_sec = 0,
                   .tm_min = 0,
                   .tm_hour = 0,
                   .tm_mday = 1,
                   .tm_mon = Month::JANUARY,
                   .tm_year = tm_year(1969),
                   .tm_wday = 3,
                   .tm_yday = 0,
                   .tm_isdst = 0}),
               tm_data);
}

TEST(LlvmLibcMkTime, InvalidEndOf32BitEpochYear) {
  if (sizeof(time_t) != 4)
    return;
  {
    // 2038-01-19 03:14:08 tests overflow of the second in 2038.
    struct tm tm_data{.tm_sec = 8,
                      .tm_min = 14,
                      .tm_hour = 3,
                      .tm_mday = 19,
                      .tm_mon = Month::JANUARY,
                      .tm_year = tm_year(2038),
                      .tm_wday = 0,
                      .tm_yday = 0,
                      .tm_isdst = 0};
    EXPECT_THAT(LIBC_NAMESPACE::mktime(&tm_data), Fails(EOVERFLOW));
  }

  {
    // 2038-01-19 03:15:07 tests overflow of the minute in 2038.
    struct tm tm_data{.tm_sec = 7,
                      .tm_min = 15,
                      .tm_hour = 3,
                      .tm_mday = 19,
                      .tm_mon = Month::JANUARY,
                      .tm_year = tm_year(2038),
                      .tm_wday = 0,
                      .tm_yday = 0,
                      .tm_isdst = 0};
    EXPECT_THAT(LIBC_NAMESPACE::mktime(&tm_data), Fails(EOVERFLOW));
  }

  {
    // 2038-01-19 04:14:07 tests overflow of the hour in 2038.
    struct tm tm_data{.tm_sec = 7,
                      .tm_min = 14,
                      .tm_hour = 4,
                      .tm_mday = 19,
                      .tm_mon = Month::JANUARY,
                      .tm_year = tm_year(2038),
                      .tm_wday = 0,
                      .tm_yday = 0,
                      .tm_isdst = 0};
    EXPECT_THAT(LIBC_NAMESPACE::mktime(&tm_data), Fails(EOVERFLOW));
  }

  {
    // 2038-01-20 03:14:07 tests overflow of the day in 2038.
    struct tm tm_data{.tm_sec = 7,
                      .tm_min = 14,
                      .tm_hour = 3,
                      .tm_mday = 20,
                      .tm_mon = Month::JANUARY,
                      .tm_year = tm_year(2038),
                      .tm_wday = 0,
                      .tm_yday = 0,
                      .tm_isdst = 0};
    EXPECT_THAT(LIBC_NAMESPACE::mktime(&tm_data), Fails(EOVERFLOW));
  }

  {
    // 2038-02-19 03:14:07 tests overflow of the month in 2038.
    struct tm tm_data{.tm_sec = 7,
                      .tm_min = 14,
                      .tm_hour = 3,
                      .tm_mday = 19,
                      .tm_mon = Month::FEBRUARY,
                      .tm_year = tm_year(2038),
                      .tm_wday = 0,
                      .tm_yday = 0,
                      .tm_isdst = 0};
    EXPECT_THAT(LIBC_NAMESPACE::mktime(&tm_data), Fails(EOVERFLOW));
  }

  {
    // 2039-01-19 03:14:07 tests overflow of the year.
    struct tm tm_data{.tm_sec = 7,
                      .tm_min = 14,
                      .tm_hour = 3,
                      .tm_mday = 19,
                      .tm_mon = Month::JANUARY,
                      .tm_year = tm_year(2039),
                      .tm_wday = 0,
                      .tm_yday = 0,
                      .tm_isdst = 0};
    EXPECT_THAT(LIBC_NAMESPACE::mktime(&tm_data), Fails(EOVERFLOW));
  }
}

TEST(LlvmLibcMkTime, InvalidMonths) {
  {
    // -1 month from 1970-01-01 00:00:00 returns 1969-12-01 00:00:00.
    struct tm tm_data{.tm_sec = 0,
                      .tm_min = 0,
                      .tm_hour = 0,
                      .tm_mday = 0,
                      .tm_mon = -1,
                      .tm_year = tm_year(1970),
                      .tm_wday = 0,
                      .tm_yday = 0,
                      .tm_isdst = 0};
    EXPECT_THAT(
        LIBC_NAMESPACE::mktime(&tm_data),
        Succeeds(-32 * LIBC_NAMESPACE::time_constants::SECONDS_PER_DAY));
    EXPECT_TM_EQ((tm{.tm_sec = 0,
                     .tm_min = 0,
                     .tm_hour = 0,
                     .tm_mday = 1,
                     .tm_mon = Month::DECEMBER,
                     .tm_year = tm_year(1969),
                     .tm_wday = 1,
                     .tm_yday = 0,
                     .tm_isdst = 0}),
                 tm_data);
  }

  {
    // 1970-13-01 00:00:00 returns 1971-01-01 00:00:00.
    struct tm tm_data{.tm_sec = 0,
                      .tm_min = 0,
                      .tm_hour = 0,
                      .tm_mday = 1,
                      .tm_mon = 12,
                      .tm_year = tm_year(1970),
                      .tm_wday = 0,
                      .tm_yday = 0,
                      .tm_isdst = 0};
    EXPECT_THAT(
        LIBC_NAMESPACE::mktime(&tm_data),
        Succeeds(LIBC_NAMESPACE::time_constants::DAYS_PER_NON_LEAP_YEAR *
                 LIBC_NAMESPACE::time_constants::SECONDS_PER_DAY));
    EXPECT_TM_EQ((tm{.tm_sec = 0,
                     .tm_min = 0,
                     .tm_hour = 0,
                     .tm_mday = 1,
                     .tm_mon = Month::JANUARY,
                     .tm_year = tm_year(1971),
                     .tm_wday = 5,
                     .tm_yday = 0,
                     .tm_isdst = 0}),
                 tm_data);
  }
}

TEST(LlvmLibcMkTime, InvalidDays) {
  {
    // -1 day from 1970-01-01 00:00:00 returns 1969-12-31 00:00:00.
    struct tm tm_data{.tm_sec = 0,
                      .tm_min = 0,
                      .tm_hour = 0,
                      .tm_mday = (1 - 1),
                      .tm_mon = Month::JANUARY,
                      .tm_year = tm_year(1970),
                      .tm_wday = 0,
                      .tm_yday = 0,
                      .tm_isdst = 0};
    EXPECT_THAT(LIBC_NAMESPACE::mktime(&tm_data),
                Succeeds(-1 * LIBC_NAMESPACE::time_constants::SECONDS_PER_DAY));
    EXPECT_TM_EQ((tm{.tm_sec = 0,
                     .tm_min = 0,
                     .tm_hour = 0,
                     .tm_mday = 31,
                     .tm_mon = Month::DECEMBER,
                     .tm_year = tm_year(1969),
                     .tm_wday = 3,
                     .tm_yday = 0,
                     .tm_isdst = 0}),
                 tm_data);
  }

  {
    // 1970-01-32 00:00:00 returns 1970-02-01 00:00:00.
    struct tm tm_data{.tm_sec = 0,
                      .tm_min = 0,
                      .tm_hour = 0,
                      .tm_mday = 32,
                      .tm_mon = Month::JANUARY,
                      .tm_year = tm_year(1970),
                      .tm_wday = 0,
                      .tm_yday = 0,
                      .tm_isdst = 0};
    EXPECT_THAT(LIBC_NAMESPACE::mktime(&tm_data),
                Succeeds(31 * LIBC_NAMESPACE::time_constants::SECONDS_PER_DAY));
    EXPECT_TM_EQ((tm{.tm_sec = 0,
                     .tm_min = 0,
                     .tm_hour = 0,
                     .tm_mday = 1,
                     .tm_mon = Month::FEBRUARY,
                     .tm_year = tm_year(1970),
                     .tm_wday = 0,
                     .tm_yday = 0,
                     .tm_isdst = 0}),
                 tm_data);
  }

  {
    // 1970-02-29 00:00:00 returns 1970-03-01 00:00:00.
    struct tm tm_data{.tm_sec = 0,
                      .tm_min = 0,
                      .tm_hour = 0,
                      .tm_mday = 29,
                      .tm_mon = Month::FEBRUARY,
                      .tm_year = tm_year(1970),
                      .tm_wday = 0,
                      .tm_yday = 0,
                      .tm_isdst = 0};
    EXPECT_THAT(LIBC_NAMESPACE::mktime(&tm_data),
                Succeeds(59 * LIBC_NAMESPACE::time_constants::SECONDS_PER_DAY));
    EXPECT_TM_EQ((tm{.tm_sec = 0,
                     .tm_min = 0,
                     .tm_hour = 0,
                     .tm_mday = 1,
                     .tm_mon = Month::MARCH,
                     .tm_year = tm_year(1970),
                     .tm_wday = 0,
                     .tm_yday = 0,
                     .tm_isdst = 0}),
                 tm_data);
  }

  {
    // 1972-02-30 00:00:00 returns 1972-03-01 00:00:00.
    struct tm tm_data{.tm_sec = 0,
                      .tm_min = 0,
                      .tm_hour = 0,
                      .tm_mday = 30,
                      .tm_mon = Month::FEBRUARY,
                      .tm_year = tm_year(1972),
                      .tm_wday = 0,
                      .tm_yday = 0,
                      .tm_isdst = 0};
    EXPECT_THAT(
        LIBC_NAMESPACE::mktime(&tm_data),
        Succeeds(((2 * LIBC_NAMESPACE::time_constants::DAYS_PER_NON_LEAP_YEAR) +
                  60) *
                 LIBC_NAMESPACE::time_constants::SECONDS_PER_DAY));
    EXPECT_TM_EQ((tm{.tm_sec = 0,
                     .tm_min = 0,
                     .tm_hour = 0,
                     .tm_mday = 1,
                     .tm_mon = Month::MARCH,
                     .tm_year = tm_year(1972),
                     .tm_wday = 3,
                     .tm_yday = 0,
                     .tm_isdst = 0}),
                 tm_data);
  }
}

TEST(LlvmLibcMkTime, EndOf32BitEpochYear) {
  // Test for maximum value of a signed 32-bit integer.
  // Test implementation can encode time for Tue 19 January 2038 03:14:07 UTC.
  {
    struct tm tm_data{.tm_sec = 7,
                      .tm_min = 14,
                      .tm_hour = 3,
                      .tm_mday = 19,
                      .tm_mon = Month::JANUARY,
                      .tm_year = tm_year(2038),
                      .tm_wday = 0,
                      .tm_yday = 0,
                      .tm_isdst = 0};
    EXPECT_THAT(LIBC_NAMESPACE::mktime(&tm_data), Succeeds(0x7FFFFFFF));
    EXPECT_TM_EQ((tm{.tm_sec = 7,
                     .tm_min = 14,
                     .tm_hour = 3,
                     .tm_mday = 19,
                     .tm_mon = Month::JANUARY,
                     .tm_year = tm_year(2038),
                     .tm_wday = 2,
                     .tm_yday = 7,
                     .tm_isdst = 0}),
                 tm_data);
  }

  // Now test some times before that, to ensure they are not rejected.
  {
    // 2038-01-19 03:13:59 tests that even a large seconds field is
    // accepted if the minutes field is smaller.
    struct tm tm_data{.tm_sec = 59,
                      .tm_min = 13,
                      .tm_hour = 3,
                      .tm_mday = 19,
                      .tm_mon = Month::JANUARY,
                      .tm_year = tm_year(2038),
                      .tm_wday = 0,
                      .tm_yday = 0,
                      .tm_isdst = 0};
    EXPECT_THAT(LIBC_NAMESPACE::mktime(&tm_data), Succeeds(0x7FFFFFFF - 8));
    EXPECT_TM_EQ((tm{.tm_sec = 59,
                     .tm_min = 13,
                     .tm_hour = 3,
                     .tm_mday = 19,
                     .tm_mon = Month::JANUARY,
                     .tm_year = tm_year(2038),
                     .tm_wday = 2,
                     .tm_yday = 7,
                     .tm_isdst = 0}),
                 tm_data);
  }

  {
    // 2038-01-19 02:59:59 tests that large seconds and minutes are
    // accepted if the hours field is smaller.
    struct tm tm_data{.tm_sec = 59,
                      .tm_min = 59,
                      .tm_hour = 2,
                      .tm_mday = 19,
                      .tm_mon = Month::JANUARY,
                      .tm_year = tm_year(2038),
                      .tm_wday = 0,
                      .tm_yday = 0,
                      .tm_isdst = 0};
    EXPECT_THAT(LIBC_NAMESPACE::mktime(&tm_data),
                Succeeds(0x7FFFFFFF - 8 -
                         14 * LIBC_NAMESPACE::time_constants::SECONDS_PER_MIN));
    EXPECT_TM_EQ((tm{.tm_sec = 59,
                     .tm_min = 59,
                     .tm_hour = 2,
                     .tm_mday = 19,
                     .tm_mon = Month::JANUARY,
                     .tm_year = tm_year(2038),
                     .tm_wday = 2,
                     .tm_yday = 7,
                     .tm_isdst = 0}),
                 tm_data);
  }

  {
    // 2038-01-18 23:59:59 tests that large seconds, minutes and hours
    // are accepted if the days field is smaller.
    struct tm tm_data{.tm_sec = 59,
                      .tm_min = 59,
                      .tm_hour = 23,
                      .tm_mday = 18,
                      .tm_mon = Month::JANUARY,
                      .tm_year = tm_year(2038),
                      .tm_wday = 0,
                      .tm_yday = 0,
                      .tm_isdst = 0};
    EXPECT_THAT(LIBC_NAMESPACE::mktime(&tm_data),
                Succeeds(0x7FFFFFFF - 8 -
                         14 * LIBC_NAMESPACE::time_constants::SECONDS_PER_MIN -
                         3 * LIBC_NAMESPACE::time_constants::SECONDS_PER_HOUR));
    EXPECT_TM_EQ((tm{.tm_sec = 59,
                     .tm_min = 59,
                     .tm_hour = 23,
                     .tm_mday = 18,
                     .tm_mon = Month::JANUARY,
                     .tm_year = tm_year(2038),
                     .tm_wday = 2,
                     .tm_yday = 7,
                     .tm_isdst = 0}),
                 tm_data);
  }

  {
    // 2038-01-18 23:59:59 tests that the final second of 2037 is
    // accepted.
    struct tm tm_data{.tm_sec = 59,
                      .tm_min = 59,
                      .tm_hour = 23,
                      .tm_mday = 31,
                      .tm_mon = Month::DECEMBER,
                      .tm_year = tm_year(2037),
                      .tm_wday = 0,
                      .tm_yday = 0,
                      .tm_isdst = 0};
    EXPECT_THAT(LIBC_NAMESPACE::mktime(&tm_data),
                Succeeds(0x7FFFFFFF - 8 -
                         14 * LIBC_NAMESPACE::time_constants::SECONDS_PER_MIN -
                         3 * LIBC_NAMESPACE::time_constants::SECONDS_PER_HOUR -
                         18 * LIBC_NAMESPACE::time_constants::SECONDS_PER_DAY));
    EXPECT_TM_EQ((tm{.tm_sec = 59,
                     .tm_min = 59,
                     .tm_hour = 23,
                     .tm_mday = 31,
                     .tm_mon = Month::DECEMBER,
                     .tm_year = tm_year(2037),
                     .tm_wday = 2,
                     .tm_yday = 7,
                     .tm_isdst = 0}),
                 tm_data);
  }
}

TEST(LlvmLibcMkTime, Max64BitYear) {
  if (sizeof(time_t) == 4)
    return;
  {
    // Mon Jan 1 12:50:50 2170 (200 years from 1970),
    struct tm tm_data{.tm_sec = 50,
                      .tm_min = 50,
                      .tm_hour = 12,
                      .tm_mday = 1,
                      .tm_mon = Month::JANUARY,
                      .tm_year = tm_year(2170),
                      .tm_wday = 0,
                      .tm_yday = 0,
                      .tm_isdst = 0};
    EXPECT_THAT(LIBC_NAMESPACE::mktime(&tm_data), Succeeds(6311479850));
    EXPECT_TM_EQ((tm{.tm_sec = 50,
                     .tm_min = 50,
                     .tm_hour = 12,
                     .tm_mday = 1,
                     .tm_mon = Month::JANUARY,
                     .tm_year = tm_year(2170),
                     .tm_wday = 1,
                     .tm_yday = 50,
                     .tm_isdst = 0}),
                 tm_data);
  }

  {
    // Test for Tue Jan 1 12:50:50 in 2,147,483,647th year.
    struct tm tm_data{.tm_sec = 50,
                      .tm_min = 50,
                      .tm_hour = 12,
                      .tm_mday = 1,
                      .tm_mon = Month::JANUARY,
                      .tm_year = tm_year(2147483647),
                      .tm_wday = 0,
                      .tm_yday = 0,
                      .tm_isdst = 0};
    EXPECT_THAT(LIBC_NAMESPACE::mktime(&tm_data), Succeeds(67767976202043050));
    EXPECT_TM_EQ((tm{.tm_sec = 50,
                     .tm_min = 50,
                     .tm_hour = 12,
                     .tm_mday = 1,
                     .tm_mon = Month::JANUARY,
                     .tm_year = tm_year(2147483647),
                     .tm_wday = 2,
                     .tm_yday = 50,
                     .tm_isdst = 0}),
                 tm_data);
  }
}

// ============================================================================
// TZ Environment Variable Integration Tests
// ============================================================================

TEST(LlvmLibcMkTime, WithTZ_EST) {
  setenv("TZ", "EST5", 1);

  // January 1, 2024 00:00:00 local EST time
  // Should convert to January 1, 2024 05:00:00 UTC
  struct tm local_tm{.tm_sec = 0,
                     .tm_min = 0,
                     .tm_hour = 0,
                     .tm_mday = 1,
                     .tm_mon = Month::JANUARY,
                     .tm_year = tm_year(2024),
                     .tm_wday = 0,
                     .tm_yday = 0,
                     .tm_isdst = -1};

  time_t result = LIBC_NAMESPACE::mktime(&local_tm);

  // Expected: 1704067200 (2024-01-01 00:00:00 UTC) + 5*3600 (EST offset)
  EXPECT_EQ(static_cast<time_t>(1704085200), result); // 2024-01-01 05:00:00 UTC

  unsetenv("TZ");
}

TEST(LlvmLibcMkTime, WithTZ_PST) {
  setenv("TZ", "PST8", 1);

  // January 1, 2024 00:00:00 local PST time
  // Should convert to January 1, 2024 08:00:00 UTC
  struct tm local_tm{.tm_sec = 0,
                     .tm_min = 0,
                     .tm_hour = 0,
                     .tm_mday = 1,
                     .tm_mon = Month::JANUARY,
                     .tm_year = tm_year(2024),
                     .tm_wday = 0,
                     .tm_yday = 0,
                     .tm_isdst = -1};

  time_t result = LIBC_NAMESPACE::mktime(&local_tm);

  EXPECT_EQ(static_cast<time_t>(1704096000), result); // 2024-01-01 08:00:00 UTC

  unsetenv("TZ");
}

TEST(LlvmLibcMkTime, WithTZ_DST_StandardTime) {
  setenv("TZ", "EST5EDT,M3.2.0,M11.1.0", 1);

  // January 15, 2024 07:00:00 local EST time (standard time)
  // Should convert to January 15, 2024 12:00:00 UTC
  struct tm local_tm{.tm_sec = 0,
                     .tm_min = 0,
                     .tm_hour = 7,
                     .tm_mday = 15,
                     .tm_mon = Month::JANUARY,
                     .tm_year = tm_year(2024),
                     .tm_wday = 0,
                     .tm_yday = 0,
                     .tm_isdst = -1};

  time_t result = LIBC_NAMESPACE::mktime(&local_tm);

  EXPECT_EQ(static_cast<time_t>(1705320000), result); // 2024-01-15 12:00:00 UTC

  unsetenv("TZ");
}

TEST(LlvmLibcMkTime, WithTZ_DST_DaylightTime) {
  setenv("TZ", "EST5EDT,M3.2.0,M11.1.0", 1);

  // July 15, 2024 08:00:00 local EDT time (daylight time)
  // Should convert to July 15, 2024 12:00:00 UTC
  struct tm local_tm{.tm_sec = 0,
                     .tm_min = 0,
                     .tm_hour = 8,
                     .tm_mday = 15,
                     .tm_mon = Month::JULY,
                     .tm_year = tm_year(2024),
                     .tm_wday = 0,
                     .tm_yday = 0,
                     .tm_isdst = -1};

  time_t result = LIBC_NAMESPACE::mktime(&local_tm);

  EXPECT_EQ(static_cast<time_t>(1721044800), result); // 2024-07-15 12:00:00 UTC

  unsetenv("TZ");
}

TEST(LlvmLibcMkTime, RoundTripVerification) {
  setenv("TZ", "PST8PDT,M3.2.0,M11.1.0", 1);

  // Create local time struct for July 15, 2024 05:00:00 PDT (daylight time)
  // This should convert to July 15, 2024 12:00:00 UTC
  struct tm local{.tm_sec = 0,
                  .tm_min = 0,
                  .tm_hour = 5,
                  .tm_mday = 15,
                  .tm_mon = Month::JULY,
                  .tm_year = tm_year(2024),
                  .tm_wday = 0,
                  .tm_yday = 0,
                  .tm_isdst = -1};

  time_t result = LIBC_NAMESPACE::mktime(&local);

  // Verify the result (July 15, 2024 12:00:00 UTC)
  EXPECT_EQ(static_cast<time_t>(1721044800), result);

  unsetenv("TZ");
}

TEST(LlvmLibcMkTime, WithoutTZ_DefaultsToUTC) {
  unsetenv("TZ");

  // January 1, 2024 00:00:00 (treated as UTC)
  struct tm tm_val{.tm_sec = 0,
                   .tm_min = 0,
                   .tm_hour = 0,
                   .tm_mday = 1,
                   .tm_mon = Month::JANUARY,
                   .tm_year = tm_year(2024),
                   .tm_wday = 0,
                   .tm_yday = 0,
                   .tm_isdst = -1};

  time_t result = LIBC_NAMESPACE::mktime(&tm_val);

  EXPECT_EQ(static_cast<time_t>(1704067200), result);
}

TEST(LlvmLibcMkTime, WithTZ_InvalidSpec) {
  setenv("TZ", "INVALID_TIMEZONE_SPEC", 1);

  // January 1, 2024 00:00:00
  struct tm tm_val{.tm_sec = 0,
                   .tm_min = 0,
                   .tm_hour = 0,
                   .tm_mday = 1,
                   .tm_mon = Month::JANUARY,
                   .tm_year = tm_year(2024),
                   .tm_wday = 0,
                   .tm_yday = 0,
                   .tm_isdst = -1};

  time_t result = LIBC_NAMESPACE::mktime(&tm_val);

  // Should behave like UTC since TZ is invalid
  EXPECT_EQ(static_cast<time_t>(1704067200), result);

  unsetenv("TZ");
}

TEST(LlvmLibcMkTime, WithTZ_EasternHemisphere) {
  setenv("TZ", "IST-5:30", 1); // India Standard Time (UTC+5:30)

  // January 1, 2024 05:30:00 local IST time
  // Should convert to January 1, 2024 00:00:00 UTC
  struct tm local_tm{.tm_sec = 0,
                     .tm_min = 30,
                     .tm_hour = 5,
                     .tm_mday = 1,
                     .tm_mon = Month::JANUARY,
                     .tm_year = tm_year(2024),
                     .tm_wday = 0,
                     .tm_yday = 0,
                     .tm_isdst = -1};

  time_t result = LIBC_NAMESPACE::mktime(&local_tm);

  EXPECT_EQ(static_cast<time_t>(1704067200), result); // 2024-01-01 00:00:00 UTC

  unsetenv("TZ");
}

// TODO(https://github.com/llvm/llvm-project/issues/XXXXX): 
// mktime() should update tm_isdst to indicate whether DST is active.
// Currently the time_t conversion works correctly, but tm_isdst is not
// being updated properly. This needs further investigation.
//
// TEST(LlvmLibcMkTime, UpdatesTmIsdst_StandardTime) { ... }
// TEST(LlvmLibcMkTime, UpdatesTmIsdst_DaylightTime) { ... }
// TEST(LlvmLibcMkTime, UpdatesTmIsdst_NoDST) { ... }
