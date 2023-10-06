//===-- Unittests for asctime ---------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/__support/CPP/string_view.h"
#include "src/time/time_zone_posix.h"
#include "test/ErrnoSetterMatcher.h"
#include "test/UnitTest/Test.h"
#include "test/src/time/TZMatcher.h"

#include <stdio.h>
#include <string.h>

using __llvm_libc::cpp::string_view;

TEST(LlvmLibcParsePosixSpec, InvalidTest) {
  const char *bad_timezones[] = {
      "",
      ":",
      " ",
      //
      // Test for [+|-]hh[:mm[:ss]] in STD and DST offsets.
      //
      // Test missing hours in STD offset.
      "EST",
      "EST+",
      "EST-",
      // Test for missing minutes in STD offset
      "EST5:",
      "EST+5:",
      "EST-5:",
      // Test for missing seconds in STD offset
      "EST5:0:",
      "EST+5:0:",
      "EST-5:0:",
      // Test invalid hours data in STD Offset.
      "EST25",
      "EST+25",
      // Test invalid minutes data in STD offset.
      "EST5:-1:0",
      "EST5:60:0",
      "EST+5:-1:0",
      "EST+5:60:0",
      "EST-5:-1:0",
      "EST-5:60:0",
      // Test invalid seconds data in STD offset.
      "EST5:0:-1",
      "EST5:0:60",
      "EST+5:0:-1",
      "EST+5:0:60",
      "EST-5:0:-1",
      "EST-5:0:60",

      // Test missing hours in DST offset.
      "EST5EDT",
      "EST5EDT+",
      "EST5EDT-",
      // Test for missing minutes in DST offset
      "EST5EDT6:",
      "EST5EDT+6:",
      "EST5EDT-6:",
      // Test for missing seconds in DST offset
      "EST5EDT6:0:",
      "EST5EDT+6:0:",
      "EST5EDT-6:0:",
      // Test invalid hours data in DST Offset.
      "EST5EDT25",
      "EST5EDT+25",
      // Test invalid minutes data in DST offset.
      "EST5EDT6:-1:0",
      "EST5EDT6:60:0",
      "EST5EDT+6:-1:0",
      "EST5EDT+6:60:0",
      "EST5EDT-6:-1:0",
      "EST5EDT-6:60:0",
      // Test invalid seconds data in DST offset.
      "EST5EDT6:0:-1",
      "EST5EDT6:0:60",
      "EST5EDT+6:0:-1",
      "EST5EDT+6:0:60",
      "EST5EDT-6:0:-1",
      "EST5EDT-6:0:60",

      // Test extra unnecessary data at the end of DST offset
      // (start_date and end_date or optional)
      "EST5EDT6AAA",
      "EST5EDT+6AAA",
      "EST5EDT-6AAA",
      "EST5EDT6:59BBB",
      "EST5EDT+6:59BBB",
      "EST5EDT-6:59BBB",
      "EST5EDT6:59:59CCC",
      "EST5EDT+6:59:59CCC",
      "EST5EDT-6:59:59CCC",

      // Test invalid time offset in start_date
      "PST8PDT,",
      "PST8PDT,M3",
      "PST8PDT,M3.",
      "PST8PDT,M3.2",
      "PST8PDT,M3.2.",
      "PST8PDT,M3.2.0,",
      "PST8PDT,M3.2.0/",
      "PST8PDT,M3.2.0/24:",
      "PST8PDT,M3.2.0/24:59:",
      "PST8PDT,M3.2.0/168",
      "PST8PDT,M3.2.0/+168",
      "PST8PDT,M3.2.0/-168",
      "PST8PDT,M3.2.0/24:-1:59",
      "PST8PDT,M3.2.0/24:60:59",
      "PST8PDT,M3.2.0/24:0:-1",
      "PST8PDT,M3.2.0/24:0:60",
      // Test invalid time offset in end_date
      "PST8PDT,M3.2.0,M11",
      "PST8PDT,M3.2.0,M11.",
      "PST8PDT,M3.2.0,M11.1",
      "PST8PDT,M3.2.0,M11.1.",
      "PST8PDT,M3.2.0,M11.1.0/",
      "PST8PDT,M3.2.0,M11.1.0/24:",
      "PST8PDT,M3.2.0,M11.1.0/24:59:",
      "PST8PDT,M3.2.0,M11.1.0/168",
      "PST8PDT,M3.2.0,M11.1.0/+168",
      "PST8PDT,M3.2.0,M11.1.0/-168",
      "PST8PDT,M3.2.0,M11.1.0/24:-1:59",
      "PST8PDT,M3.2.0,M11.1.0/24:60:59",
      "PST8PDT,M3.2.0,M11.1.0/24:0:-1",
      "PST8PDT,M3.2.0,M11.1.0/24:0:60",

      // Test invalid data in MonthWeekWeekday
      "PST8PDT,M0.2.0,M11.1.0",   // Test 0 as month in start_date
      "PST8PDT,M13.2.0,M11.1.0",  // Test 13 as month in start_date
      "PST8PDT,M1.0.0,M11.1.0",   // Test 0 as week in start_date
      "PST8PDT,M1.6.0,M11.1.0",   // Test 6 as week in start_date
      "PST8PDT,M1.2.-1,M11.1.0",  // Test -1 as weekday in start_date
      "PST8PDT,M1.2.7,M11.1.0",   // Test 7 as weekday in start_date
      "PST8PDT,M0.2.0,M0.1.0",    // Test 0 as month in end_date
      "PST8PDT,M13.2.0,M13.1.0",  // Test 13 as month in end_date
      "PST8PDT,M1.0.0,M11.0.0",   // Test 0 as week in end_date
      "PST8PDT,M1.6.0,M11.6.0",   // Test 6 as week in end_date
      "PST8PDT,M1.2.-1,M11.1.-1", // Test -1 as weekday in end_date
      "PST8PDT,M1.2.7,M11.1.7",   // Test 7 as weekday in end_date
      "PST8PDT,J0",               // Test 0 as nonLeapDay in start_date
      "PST8PDT,J366",             // Test 366 as nonLeapDay in start_date
      "PST8PDT,J1,J0",            // Test 0 as nonLeapDay in end_date
      "PST8PDT,J1,J366",          // Test 366 as nonLeapDay in end_date
      "PST8PDT,-1",               // Test -1 as LeapDay in start_date
      "PST8PDT,366",              // Test 366 as LeapDay in start_date
      "PST8PDT,1,-1",             // Test -1 as LeapDay in end_date
      "PST8PDT,1,366",            // Test 366 as LeapDay in end_date

      // Test extra unnecessary data at the end.
      "PST8PDT,M3.2.0,M11.1.0AA",
      "PST8PDT,M3.2.0BB",
      "PST8PDT,J59CC",
      "PST8PDT,J59,J58DD",
      "PST8PDT,59EE",
      "PST8PDT,59,58FF",
  };

  for (const auto &timezone : bad_timezones) {
    __llvm_libc::cpp::string_view timezone_spec(timezone);
    const auto posix_result =
        __llvm_libc::time_zone_posix::PosixTimeZone::ParsePosixSpec(
            timezone_spec);
    // TODO(rtenneti): Use a special matcher to verify the data.
    if (posix_result.has_value()) {
      __builtin_printf(
          "Testing failed for: %s - expected to faile but succeed.\n",
          timezone);
    }
    ASSERT_FALSE(posix_result.has_value());
  }
}

TEST(LlvmLibcParsePosixSpec, ValidTest) {
  struct __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData
      good_timezones[] = {
          // [Pacific/Honolulu]
          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "HST10",
              .std_abbr = "HST",
              .std_offset = -36000,
              .dst_abbr = "",
              .dst_offset = 0},
          // [Asia/Beijing]
          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "CST-8",
              .std_abbr = "CST",
              .std_offset = 28800,
              .dst_abbr = "",
              .dst_offset = 0},
          // [America/New_York]
          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "EST5EDT,M3.2.0/2,M11.1.0",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "EDT",
              .dst_offset = -14400},
          // [Europe/Paris]
          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "CET-1CEST,M3.5.0/2,M10.5.0/3",
              .std_abbr = "CET",
              .std_offset = 3600,
              .dst_abbr = "CEST",
              .dst_offset = 7200},

          // [America/St_Johns]
          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "NST03:30NDT,M3.2.0/0:01,M11.1.0/0:01",
              .std_abbr = "NST",
              .std_offset = -12600,
              .dst_abbr = "NDT",
              .dst_offset = -9000},

          // [Atlantis/Foobar]
          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "AST2:45ADT0:45,M4.1.6/1:45,M10.5.6/2:45",
              .std_abbr = "AST",
              .std_offset = -9900,
              .dst_abbr = "ADT",
              .dst_offset = -2700},

          //
          // We need to verify the data by setting the TZ and calling localtime.
          //
          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "EST5",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "",
              .dst_offset = 0},

          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "EST5:0",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "",
              .dst_offset = 0},

          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "EST5:59",
              .std_abbr = "EST",
              .std_offset = -21540,
              .dst_abbr = "",
              .dst_offset = 0},

          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "EST5:0:0",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "",
              .dst_offset = 0},

          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "EST5:0:59",
              .std_abbr = "EST",
              .std_offset = -18059,
              .dst_abbr = "",
              .dst_offset = 0},

          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "EST+5",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "",
              .dst_offset = 0},

          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "EST+5:0",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "",
              .dst_offset = 0},

          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "EST+5:59",
              .std_abbr = "EST",
              .std_offset = -21540,
              .dst_abbr = "",
              .dst_offset = 0},

          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "EST+5:0:0",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "",
              .dst_offset = 0},

          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "EST+5:0:59",
              .std_abbr = "EST",
              .std_offset = -18059,
              .dst_abbr = "",
              .dst_offset = 0},

          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "EST-5",
              .std_abbr = "EST",
              .std_offset = 18000,
              .dst_abbr = "",
              .dst_offset = 0},

          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "EST-5:0",
              .std_abbr = "EST",
              .std_offset = 18000,
              .dst_abbr = "",
              .dst_offset = 0},

          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "EST-5:59",
              .std_abbr = "EST",
              .std_offset = 21540,
              .dst_abbr = "",
              .dst_offset = 0},

          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "EST-5:0:0",
              .std_abbr = "EST",
              .std_offset = 18000,
              .dst_abbr = "",
              .dst_offset = 0},

          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "EST-5:0:59",
              .std_abbr = "EST",
              .std_offset = 18059,
              .dst_abbr = "",
              .dst_offset = 0},

          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "EST5EDT6",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "EDT",
              .dst_offset = -21600},

          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "EST5EDT6:0",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "EDT",
              .dst_offset = -21600},

          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "EST5EDT6:59",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "EDT",
              .dst_offset = -25140},

          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "EST5EDT6:0:0",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "EDT",
              .dst_offset = -21600},

          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "EST5EDT6:0:59",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "EDT",
              .dst_offset = -21659},

          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "EST5EDT+6",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "EDT",
              .dst_offset = -21600},

          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "EST5EDT+6:0",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "EDT",
              .dst_offset = -21600},

          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "EST5EDT+6:59",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "EDT",
              .dst_offset = -25140},

          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "EST5EDT+6:0:0",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "EDT",
              .dst_offset = -21600},

          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "EST5EDT+6:0:59",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "EDT",
              .dst_offset = -21659},

          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "EST5EDT-6",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "EDT",
              .dst_offset = 21600},

          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "EST5EDT-6/1:2:3",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "EDT",
              .dst_offset = 21600},

          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "EST5EDT-6:0",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "EDT",
              .dst_offset = 21600},

          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "EST5EDT-6:0/1:2:3",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "EDT",
              .dst_offset = 21600},

          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "EST5EDT-6:59",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "EDT",
              .dst_offset = 25140},

          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "EST5EDT-6:59/1:2:3",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "EDT",
              .dst_offset = 25140},

          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "EST5EDT-6:0:0",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "EDT",
              .dst_offset = 21600},

          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "EST5EDT-6:0:0/1:2:3",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "EDT",
              .dst_offset = 21600},

          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "EST5EDT-6:0:59",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "EDT",
              .dst_offset = 21659},

          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "EST5EDT-6:0:59/1:2:3",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "EDT",
              .dst_offset = 21659},
          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "PST8PDT,M3.2.0",
              .std_abbr = "PST",
              .std_offset = -28800,
              .dst_abbr = "PDT",
              .dst_offset = -25200},
          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "PST8PDT,M3.2.0/1:2:3",
              .std_abbr = "PST",
              .std_offset = -28800,
              .dst_abbr = "PDT",
              .dst_offset = -25200},
          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "PST8PDT,M3.2.0,M11.1.0",
              .std_abbr = "PST",
              .std_offset = -28800,
              .dst_abbr = "PDT",
              .dst_offset = -25200},
          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "PST8PDT,M3.2.0,M11.1.0/1:2:3",
              .std_abbr = "PST",
              .std_offset = -28800,
              .dst_abbr = "PDT",
              .dst_offset = -25200},
          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "PST8PDT,J59",
              .std_abbr = "PST",
              .std_offset = -28800,
              .dst_abbr = "PDT",
              .dst_offset = -25200},
          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "PST8PDT,J59/1:2:3",
              .std_abbr = "PST",
              .std_offset = -28800,
              .dst_abbr = "PDT",
              .dst_offset = -25200},
          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "PST8PDT,J59,J58",
              .std_abbr = "PST",
              .std_offset = -28800,
              .dst_abbr = "PDT",
              .dst_offset = -25200},
          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "PST8PDT,J59,J58/1:2:3",
              .std_abbr = "PST",
              .std_offset = -28800,
              .dst_abbr = "PDT",
              .dst_offset = -25200},
          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "PST8PDT,59",
              .std_abbr = "PST",
              .std_offset = -28800,
              .dst_abbr = "PDT",
              .dst_offset = -25200},
          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "PST8PDT,59/1:2:3",
              .std_abbr = "PST",
              .std_offset = -28800,
              .dst_abbr = "PDT",
              .dst_offset = -25200},
          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "PST8PDT,59,58",
              .std_abbr = "PST",
              .std_offset = -28800,
              .dst_abbr = "PDT",
              .dst_offset = -25200},
          __llvm_libc::tzmatcher::testing::PosixTimeZoneTestData{
              .spec = "PST8PDT,59,58/1:2:3",
              .std_abbr = "PST",
              .std_offset = -28800,
              .dst_abbr = "PDT",
              .dst_offset = -25200},
      };

  for (const auto &timezone_test_data : good_timezones) {
    __llvm_libc::cpp::string_view timezone_spec(timezone_test_data.spec);
    const auto posix_result =
        __llvm_libc::time_zone_posix::PosixTimeZone::ParsePosixSpec(
            timezone_spec);
    ASSERT_TRUE(posix_result.has_value());
    __llvm_libc::time_zone_posix::PosixTimeZone posix = posix_result.value();

    EXPECT_POSIX_TIME_ZONE_TEST_DATA_EQ(timezone_test_data, posix);
  }
}

TEST(LlvmLibcParsePosixSpec, ValidTestAndVerify) {
  __llvm_libc::cpp::string_view timezone_spec("PST8PDT,M3.2.0,M11.1.0");
  // TODO(rtenneti): Use a special matcher to verify the data.
  __llvm_libc::cpp::optional<__llvm_libc::time_zone_posix::PosixTimeZone>
      posix_result =
          __llvm_libc::time_zone_posix::PosixTimeZone::ParsePosixSpec(
              timezone_spec);
  ASSERT_TRUE(posix_result.has_value());
  __llvm_libc::time_zone_posix::PosixTimeZone posix = posix_result.value();

  __llvm_libc::time_zone_posix::PosixTransition dst_start(
      __llvm_libc::time_zone_posix::PosixTransition::DateFormat::M,
      static_cast<int8_t>(3), static_cast<int8_t>(2), static_cast<int8_t>(0),
      static_cast<int32_t>(7200));

  __llvm_libc::time_zone_posix::PosixTransition dst_end(
      __llvm_libc::time_zone_posix::PosixTransition::DateFormat::M,
      static_cast<int8_t>(11), static_cast<int8_t>(1), static_cast<int8_t>(0),
      static_cast<int32_t>(7200));

  __llvm_libc::time_zone_posix::PosixTimeZone expected_posix(
      __llvm_libc::cpp::string_view(""), __llvm_libc::cpp::string_view("PST"),
      static_cast<int32_t>(-28800), __llvm_libc::cpp::string_view("PDT"),
      static_cast<int32_t>(-25200), dst_start, dst_end);

  EXPECT_POSIX_TIME_ZONE_EQ(expected_posix, posix);
}
