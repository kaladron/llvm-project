//===-- Unittests for asctime ---------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/__support/CPP/string_view.h"
#include "src/time/time_zone_posix.h"
#include "test/UnitTest/ErrnoSetterMatcher.h"
#include "test/UnitTest/Test.h"
#include "test/src/time/TZMatcher.h"

#include <stdio.h>
#include <string.h>

using LIBC_NAMESPACE::cpp::string_view;

TEST(LlvmLibcParsePosixSpec, ParserBasicTest) {
  // Test that Parser can be created and basic operations work
  using Parser = LIBC_NAMESPACE::time_zone_posix::PosixTimeZone::Parser;
  
  const char* test_spec = "EST5EDT,M3.2.0,M11.1.0";
  Parser parser{string_view(test_spec)};
  
  // Verify initial state
  ASSERT_TRUE(parser.has_more());
  EXPECT_EQ(parser.current_position(), static_cast<size_t>(0));
  EXPECT_STREQ(parser.get_remaining().data(), test_spec);
  EXPECT_STREQ(parser.get_original().data(), test_spec);
  
  // Advance by 3 characters (skip "EST")
  parser.advance(3);
  EXPECT_EQ(parser.current_position(), static_cast<size_t>(3));
  EXPECT_TRUE(parser.has_more());
  EXPECT_EQ(parser.get_remaining().size(), strlen(test_spec) - 3);
  
  // Original should remain unchanged
  EXPECT_STREQ(parser.get_original().data(), test_spec);
  EXPECT_EQ(parser.get_original().size(), strlen(test_spec));
  
  // Advance to the end
  parser.advance(parser.get_remaining().size());
  EXPECT_FALSE(parser.has_more());
  EXPECT_EQ(parser.current_position(), strlen(test_spec));
  EXPECT_EQ(parser.get_remaining().size(), static_cast<size_t>(0));
  
  // Original should still be unchanged
  EXPECT_STREQ(parser.get_original().data(), test_spec);
}

TEST(LlvmLibcParsePosixSpec, ParseOffsetTest) {
  using LIBC_NAMESPACE::time_zone_posix::PosixTimeZone;
  using LIBC_NAMESPACE::time_zone_posix::TZOffset;

  // Test default negative sign (no sign means west/negative)
  {
    string_view spec = "5";
    auto result = PosixTimeZone::Parser::parse_offset(spec, 0, 24, TZOffset::NEGATIVE);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, -18000); // -5 hours in seconds
    EXPECT_TRUE(spec.empty());
  }

  // Test explicit positive sign with NEGATIVE default
  {
    string_view spec = "+5";
    auto result = PosixTimeZone::Parser::parse_offset(spec, 0, 24, TZOffset::NEGATIVE);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, -18000); // Still negative because default is NEGATIVE
    EXPECT_TRUE(spec.empty());
  }

  // Test explicit negative sign with NEGATIVE default (double negative = positive)
  {
    string_view spec = "-5";
    auto result = PosixTimeZone::Parser::parse_offset(spec, 0, 24, TZOffset::NEGATIVE);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 18000); // +5 hours in seconds (negates the default)
    EXPECT_TRUE(spec.empty());
  }

  // Test hours:minutes:seconds format
  {
    string_view spec = "5:30:45";
    auto result = PosixTimeZone::Parser::parse_offset(spec, 0, 24, TZOffset::NEGATIVE);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, -(5 * 3600 + 30 * 60 + 45)); // -5:30:45 in seconds
    EXPECT_TRUE(spec.empty());
  }

  // Test hours:minutes format (no seconds)
  {
    string_view spec = "5:30";
    auto result = PosixTimeZone::Parser::parse_offset(spec, 0, 24, TZOffset::NEGATIVE);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, -(5 * 3600 + 30 * 60)); // -5:30:00 in seconds
    EXPECT_TRUE(spec.empty());
  }

  // Test boundary: 24 hours (max for standard offset)
  {
    string_view spec = "24";
    auto result = PosixTimeZone::Parser::parse_offset(spec, 0, 24, TZOffset::NEGATIVE);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, -86400); // -24 hours in seconds
    EXPECT_TRUE(spec.empty());
  }

  // Test out of range hour (should fail)
  {
    string_view spec = "25";
    auto result = PosixTimeZone::Parser::parse_offset(spec, 0, 24, TZOffset::NEGATIVE);
    EXPECT_FALSE(result.has_value());
  }

  // Test positive offset with POSITIVE default
  {
    string_view spec = "+5:30";
    auto result = PosixTimeZone::Parser::parse_offset(spec, 0, 24, TZOffset::POSITIVE);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 5 * 3600 + 30 * 60); // +5:30:00 in seconds
    EXPECT_TRUE(spec.empty());
  }

  // Test zero offset
  {
    string_view spec = "0";
    auto result = PosixTimeZone::Parser::parse_offset(spec, 0, 24, TZOffset::NEGATIVE);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 0);
    EXPECT_TRUE(spec.empty());
  }
}

TEST(LlvmLibcParsePosixSpec, ParseDateTimeTest) {
  using LIBC_NAMESPACE::time_zone_posix::PosixTimeZone;
  using LIBC_NAMESPACE::time_zone_posix::PosixTransition;

  // Test M format: M3.2.0 (2nd Sunday in March)
  {
    string_view spec = ",M3.2.0";
    auto result = PosixTimeZone::Parser::parse_date_time(spec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->date.fmt, PosixTransition::DateFormat::M);
    auto& month_week_weekday = result->date.data.get<PosixTransition::Date::MonthWeekWeekday>();
    EXPECT_EQ(month_week_weekday.month, static_cast<int8_t>(3));
    EXPECT_EQ(month_week_weekday.week, static_cast<int8_t>(2));
    EXPECT_EQ(month_week_weekday.weekday, static_cast<int8_t>(0));
    EXPECT_EQ(result->time.offset, 7200); // Default 02:00:00
    EXPECT_TRUE(spec.empty());
  }

  // Test M format with custom time: M11.1.0/1:30:45
  {
    string_view spec = ",M11.1.0/1:30:45";
    auto result = PosixTimeZone::Parser::parse_date_time(spec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->date.fmt, PosixTransition::DateFormat::M);
    auto& month_week_weekday = result->date.data.get<PosixTransition::Date::MonthWeekWeekday>();
    EXPECT_EQ(month_week_weekday.month, static_cast<int8_t>(11));
    EXPECT_EQ(month_week_weekday.week, static_cast<int8_t>(1));
    EXPECT_EQ(month_week_weekday.weekday, static_cast<int8_t>(0));
    EXPECT_EQ(result->time.offset, 1 * 3600 + 30 * 60 + 45); // 1:30:45
    EXPECT_TRUE(spec.empty());
  }

  // Test J format: J59 (59th day, excluding leap days)
  {
    string_view spec = ",J59";
    auto result = PosixTimeZone::Parser::parse_date_time(spec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->date.fmt, PosixTransition::DateFormat::J);
    auto& non_leap_day = result->date.data.get<PosixTransition::Date::NonLeapDay>();
    EXPECT_EQ(non_leap_day.day, static_cast<int16_t>(59));
    EXPECT_EQ(result->time.offset, 7200); // Default 02:00:00
    EXPECT_TRUE(spec.empty());
  }

  // Test J format with custom time: J365/0
  {
    string_view spec = ",J365/0";
    auto result = PosixTimeZone::Parser::parse_date_time(spec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->date.fmt, PosixTransition::DateFormat::J);
    auto& non_leap_day = result->date.data.get<PosixTransition::Date::NonLeapDay>();
    EXPECT_EQ(non_leap_day.day, static_cast<int16_t>(365));
    EXPECT_EQ(result->time.offset, 0); // Midnight
    EXPECT_TRUE(spec.empty());
  }

  // Test N format: 59 (59th day, including leap days)
  {
    string_view spec = ",59";
    auto result = PosixTimeZone::Parser::parse_date_time(spec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->date.fmt, PosixTransition::DateFormat::N);
    auto& day = result->date.data.get<PosixTransition::Date::Day>();
    EXPECT_EQ(day.day, static_cast<int16_t>(59));
    EXPECT_EQ(result->time.offset, 7200); // Default 02:00:00
    EXPECT_TRUE(spec.empty());
  }

  // Test N format with custom time: 0/2:30:45
  {
    string_view spec = ",0/2:30:45";
    auto result = PosixTimeZone::Parser::parse_date_time(spec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->date.fmt, PosixTransition::DateFormat::N);
    auto& day = result->date.data.get<PosixTransition::Date::Day>();
    EXPECT_EQ(day.day, static_cast<int16_t>(0));
    EXPECT_EQ(result->time.offset, 2 * 3600 + 30 * 60 + 45); // 2:30:45
    EXPECT_TRUE(spec.empty());
  }

  // Test with negative time offset (before midnight)
  {
    string_view spec = ",M3.2.0/-1";
    auto result = PosixTimeZone::Parser::parse_date_time(spec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->date.fmt, PosixTransition::DateFormat::M);
    EXPECT_EQ(result->time.offset, -3600); // -1 hour
    EXPECT_TRUE(spec.empty());
  }

  // Test with large positive time offset (RFC 8536)
  {
    string_view spec = ",M3.2.0/167";
    auto result = PosixTimeZone::Parser::parse_date_time(spec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->time.offset, 167 * 3600); // 167 hours
    EXPECT_TRUE(spec.empty());
  }

  // Test invalid: out of range month
  {
    string_view spec = ",M13.2.0";
    auto result = PosixTimeZone::Parser::parse_date_time(spec);
    EXPECT_FALSE(result.has_value());
  }

  // Test invalid: out of range week
  {
    string_view spec = ",M3.6.0";
    auto result = PosixTimeZone::Parser::parse_date_time(spec);
    EXPECT_FALSE(result.has_value());
  }

  // Test invalid: out of range weekday
  {
    string_view spec = ",M3.2.7";
    auto result = PosixTimeZone::Parser::parse_date_time(spec);
    EXPECT_FALSE(result.has_value());
  }

  // Test invalid: J format with day 0
  {
    string_view spec = ",J0";
    auto result = PosixTimeZone::Parser::parse_date_time(spec);
    EXPECT_FALSE(result.has_value());
  }

  // Test invalid: J format with day > 365
  {
    string_view spec = ",J366";
    auto result = PosixTimeZone::Parser::parse_date_time(spec);
    EXPECT_FALSE(result.has_value());
  }

  // Test invalid: N format with day > 365
  {
    string_view spec = ",366";
    auto result = PosixTimeZone::Parser::parse_date_time(spec);
    EXPECT_FALSE(result.has_value());
  }
}

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
    LIBC_NAMESPACE::cpp::string_view timezone_spec(timezone);
    const auto posix_result =
        LIBC_NAMESPACE::time_zone_posix::PosixTimeZone::ParsePosixSpec(
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
  struct LIBC_NAMESPACE::testing::PosixTimeZoneTestData
      good_timezones[] = {
          // [Pacific/Honolulu]
          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "HST10",
              .std_abbr = "HST",
              .std_offset = -36000,
              .dst_abbr = "",
              .dst_offset = 0},
          // [Asia/Beijing]
          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "CST-8",
              .std_abbr = "CST",
              .std_offset = 28800,
              .dst_abbr = "",
              .dst_offset = 0},
          // [America/New_York]
          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "EST5EDT,M3.2.0/2,M11.1.0",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "EDT",
              .dst_offset = -14400},
          // [Europe/Paris]
          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "CET-1CEST,M3.5.0/2,M10.5.0/3",
              .std_abbr = "CET",
              .std_offset = 3600,
              .dst_abbr = "CEST",
              .dst_offset = 7200},

          // [America/St_Johns]
          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "NST03:30NDT,M3.2.0/0:01,M11.1.0/0:01",
              .std_abbr = "NST",
              .std_offset = -12600,
              .dst_abbr = "NDT",
              .dst_offset = -9000},

          // [Atlantis/Foobar]
          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "AST2:45ADT0:45,M4.1.6/1:45,M10.5.6/2:45",
              .std_abbr = "AST",
              .std_offset = -9900,
              .dst_abbr = "ADT",
              .dst_offset = -2700},

          //
          // We need to verify the data by setting the TZ and calling localtime.
          //
          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "EST5",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "",
              .dst_offset = 0},

          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "EST5:0",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "",
              .dst_offset = 0},

          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "EST5:59",
              .std_abbr = "EST",
              .std_offset = -21540,
              .dst_abbr = "",
              .dst_offset = 0},

          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "EST5:0:0",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "",
              .dst_offset = 0},

          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "EST5:0:59",
              .std_abbr = "EST",
              .std_offset = -18059,
              .dst_abbr = "",
              .dst_offset = 0},

          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "EST+5",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "",
              .dst_offset = 0},

          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "EST+5:0",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "",
              .dst_offset = 0},

          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "EST+5:59",
              .std_abbr = "EST",
              .std_offset = -21540,
              .dst_abbr = "",
              .dst_offset = 0},

          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "EST+5:0:0",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "",
              .dst_offset = 0},

          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "EST+5:0:59",
              .std_abbr = "EST",
              .std_offset = -18059,
              .dst_abbr = "",
              .dst_offset = 0},

          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "EST-5",
              .std_abbr = "EST",
              .std_offset = 18000,
              .dst_abbr = "",
              .dst_offset = 0},

          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "EST-5:0",
              .std_abbr = "EST",
              .std_offset = 18000,
              .dst_abbr = "",
              .dst_offset = 0},

          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "EST-5:59",
              .std_abbr = "EST",
              .std_offset = 21540,
              .dst_abbr = "",
              .dst_offset = 0},

          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "EST-5:0:0",
              .std_abbr = "EST",
              .std_offset = 18000,
              .dst_abbr = "",
              .dst_offset = 0},

          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "EST-5:0:59",
              .std_abbr = "EST",
              .std_offset = 18059,
              .dst_abbr = "",
              .dst_offset = 0},

          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "EST5EDT6",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "EDT",
              .dst_offset = -21600},

          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "EST5EDT6:0",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "EDT",
              .dst_offset = -21600},

          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "EST5EDT6:59",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "EDT",
              .dst_offset = -25140},

          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "EST5EDT6:0:0",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "EDT",
              .dst_offset = -21600},

          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "EST5EDT6:0:59",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "EDT",
              .dst_offset = -21659},

          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "EST5EDT+6",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "EDT",
              .dst_offset = -21600},

          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "EST5EDT+6:0",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "EDT",
              .dst_offset = -21600},

          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "EST5EDT+6:59",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "EDT",
              .dst_offset = -25140},

          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "EST5EDT+6:0:0",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "EDT",
              .dst_offset = -21600},

          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "EST5EDT+6:0:59",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "EDT",
              .dst_offset = -21659},

          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "EST5EDT-6",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "EDT",
              .dst_offset = 21600},

          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "EST5EDT-6/1:2:3",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "EDT",
              .dst_offset = 21600},

          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "EST5EDT-6:0",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "EDT",
              .dst_offset = 21600},

          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "EST5EDT-6:0/1:2:3",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "EDT",
              .dst_offset = 21600},

          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "EST5EDT-6:59",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "EDT",
              .dst_offset = 25140},

          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "EST5EDT-6:59/1:2:3",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "EDT",
              .dst_offset = 25140},

          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "EST5EDT-6:0:0",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "EDT",
              .dst_offset = 21600},

          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "EST5EDT-6:0:0/1:2:3",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "EDT",
              .dst_offset = 21600},

          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "EST5EDT-6:0:59",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "EDT",
              .dst_offset = 21659},

          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "EST5EDT-6:0:59/1:2:3",
              .std_abbr = "EST",
              .std_offset = -18000,
              .dst_abbr = "EDT",
              .dst_offset = 21659},
          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "PST8PDT,M3.2.0",
              .std_abbr = "PST",
              .std_offset = -28800,
              .dst_abbr = "PDT",
              .dst_offset = -25200},
          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "PST8PDT,M3.2.0/1:2:3",
              .std_abbr = "PST",
              .std_offset = -28800,
              .dst_abbr = "PDT",
              .dst_offset = -25200},
          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "PST8PDT,M3.2.0,M11.1.0",
              .std_abbr = "PST",
              .std_offset = -28800,
              .dst_abbr = "PDT",
              .dst_offset = -25200},
          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "PST8PDT,M3.2.0,M11.1.0/1:2:3",
              .std_abbr = "PST",
              .std_offset = -28800,
              .dst_abbr = "PDT",
              .dst_offset = -25200},
          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "PST8PDT,J59",
              .std_abbr = "PST",
              .std_offset = -28800,
              .dst_abbr = "PDT",
              .dst_offset = -25200},
          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "PST8PDT,J59/1:2:3",
              .std_abbr = "PST",
              .std_offset = -28800,
              .dst_abbr = "PDT",
              .dst_offset = -25200},
          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "PST8PDT,J59,J58",
              .std_abbr = "PST",
              .std_offset = -28800,
              .dst_abbr = "PDT",
              .dst_offset = -25200},
          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "PST8PDT,J59,J58/1:2:3",
              .std_abbr = "PST",
              .std_offset = -28800,
              .dst_abbr = "PDT",
              .dst_offset = -25200},
          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "PST8PDT,59",
              .std_abbr = "PST",
              .std_offset = -28800,
              .dst_abbr = "PDT",
              .dst_offset = -25200},
          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "PST8PDT,59/1:2:3",
              .std_abbr = "PST",
              .std_offset = -28800,
              .dst_abbr = "PDT",
              .dst_offset = -25200},
          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "PST8PDT,59,58",
              .std_abbr = "PST",
              .std_offset = -28800,
              .dst_abbr = "PDT",
              .dst_offset = -25200},
          LIBC_NAMESPACE::testing::PosixTimeZoneTestData{
              .spec = "PST8PDT,59,58/1:2:3",
              .std_abbr = "PST",
              .std_offset = -28800,
              .dst_abbr = "PDT",
              .dst_offset = -25200},
      };

  for (const auto &timezone_test_data : good_timezones) {
    LIBC_NAMESPACE::cpp::string_view timezone_spec(timezone_test_data.spec);
    const auto posix_result =
        LIBC_NAMESPACE::time_zone_posix::PosixTimeZone::ParsePosixSpec(
            timezone_spec);
    ASSERT_TRUE(posix_result.has_value());
    LIBC_NAMESPACE::time_zone_posix::PosixTimeZone posix = posix_result.value();

    EXPECT_POSIX_TIME_ZONE_TEST_DATA_EQ(timezone_test_data, posix);
  }
}

TEST(LlvmLibcParsePosixSpec, ValidTestAndVerify) {
  LIBC_NAMESPACE::cpp::string_view timezone_spec("PST8PDT,M3.2.0,M11.1.0");
  // TODO(rtenneti): Use a special matcher to verify the data.
  LIBC_NAMESPACE::cpp::optional<LIBC_NAMESPACE::time_zone_posix::PosixTimeZone>
      posix_result =
          LIBC_NAMESPACE::time_zone_posix::PosixTimeZone::ParsePosixSpec(
              timezone_spec);
  ASSERT_TRUE(posix_result.has_value());
  LIBC_NAMESPACE::time_zone_posix::PosixTimeZone posix = posix_result.value();

  LIBC_NAMESPACE::time_zone_posix::PosixTransition dst_start(
      LIBC_NAMESPACE::time_zone_posix::PosixTransition::DateFormat::M,
      static_cast<int8_t>(3), static_cast<int8_t>(2), static_cast<int8_t>(0),
      static_cast<int32_t>(7200));

  LIBC_NAMESPACE::time_zone_posix::PosixTransition dst_end(
      LIBC_NAMESPACE::time_zone_posix::PosixTransition::DateFormat::M,
      static_cast<int8_t>(11), static_cast<int8_t>(1), static_cast<int8_t>(0),
      static_cast<int32_t>(7200));

  LIBC_NAMESPACE::time_zone_posix::PosixTimeZone expected_posix(
      LIBC_NAMESPACE::cpp::string_view(""), LIBC_NAMESPACE::cpp::string_view("PST"),
      static_cast<int32_t>(-28800), LIBC_NAMESPACE::cpp::string_view("PDT"),
      static_cast<int32_t>(-25200), dst_start, dst_end);

  EXPECT_POSIX_TIME_ZONE_EQ(expected_posix, posix);
}

TEST(LlvmLibcParsePosixSpec, QuotedTimeZoneNames) {
  using LIBC_NAMESPACE::cpp::string_view;
  using LIBC_NAMESPACE::time_zone_posix::PosixTimeZone;

  // Test quoted timezone names with special characters (+ and -)
  // These are allowed inside <...> but not in unquoted abbreviations

  // Test 1: Plus sign in name
  {
    string_view spec = "<UTC+5>-5";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->std_abbr, string_view("UTC+5"));
    EXPECT_EQ(result->std_offset, 5 * 3600); // -5 becomes +5*3600
  }

  // Test 2: Minus sign in name
  {
    string_view spec = "<UTC-5>5";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->std_abbr, string_view("UTC-5"));
    EXPECT_EQ(result->std_offset, -5 * 3600);
  }

  // Test 3: Both plus and minus signs in name
  {
    string_view spec = "<A-B+C>3";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->std_abbr, string_view("A-B+C"));
    EXPECT_EQ(result->std_offset, -3 * 3600);
  }

  // Test 4: Simple quoted name (no special chars)
  {
    string_view spec = "<ABC>5";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->std_abbr, string_view("ABC"));
    EXPECT_EQ(result->std_offset, -5 * 3600);
  }

  // Test 5: Quoted name with DST and special characters
  {
    string_view spec = "<EST-5>5<EDT-4>,M3.2.0,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->std_abbr, string_view("EST-5"));
    EXPECT_EQ(result->std_offset, -5 * 3600);
    EXPECT_EQ(result->dst_abbr, string_view("EDT-4"));
    EXPECT_EQ(result->dst_offset, -4 * 3600);
  }

  // Test 6: Quoted name with digits
  {
    string_view spec = "<UTC5>-5";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->std_abbr, string_view("UTC5"));
  }

  // Test 7: Empty quoted name (should fail)
  {
    string_view spec = "<>5";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  // Test 8: Unclosed quote (should fail)
  {
    string_view spec = "<ABC5";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  // Test 9: Closing quote without opening (should fail - invalid format)
  {
    string_view spec = "ABC>5";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }
}

TEST(LlvmLibcParsePosixSpec, RFC8536ExtendedHours) {
  using LIBC_NAMESPACE::cpp::string_view;
  using LIBC_NAMESPACE::time_zone_posix::PosixTimeZone;

  // RFC 8536 extends the hour range for transition times from 0-24 to -167 to +167
  // This allows representing transitions that occur on days other than the nominal day

  // Test 1: Maximum positive hours (167:59:59)
  {
    string_view spec = "EST5EDT,M3.2.0/167:59:59,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->dst_start.time.offset, 167 * 3600 + 59 * 60 + 59);
  }

  // Test 2: Maximum negative hours (-167:00:00)
  {
    string_view spec = "EST5EDT,M3.2.0/-167:00:00,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->dst_start.time.offset, -167 * 3600);
  }

  // Test 3: Negative hours with minutes and seconds (-167:30:45)
  {
    string_view spec = "EST5EDT,M3.2.0/-167:30:45,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->dst_start.time.offset, -167 * 3600 - 30 * 60 - 45);
  }

  // Test 4: Over maximum positive (168:00:00 should fail)
  {
    string_view spec = "EST5EDT,M3.2.0/168:00:00,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  // Test 5: Under minimum negative (-168:00:00 should fail)
  {
    string_view spec = "EST5EDT,M3.2.0/-168:00:00,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  // Test 6: Old POSIX maximum (24:00:00) should still work
  {
    string_view spec = "EST5EDT,M3.2.0/24:00:00,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->dst_start.time.offset, 24 * 3600);
  }

  // Test 7: Extended hours on end transition
  {
    string_view spec = "EST5EDT,M3.2.0,M11.1.0/100:30:15";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->dst_end.time.offset, 100 * 3600 + 30 * 60 + 15);
  }

  // Test 8: Extended hours on both transitions
  {
    string_view spec = "EST5EDT,M3.2.0/-50:00:00,M11.1.0/150:00:00";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->dst_start.time.offset, -50 * 3600);
    EXPECT_EQ(result->dst_end.time.offset, 150 * 3600);
  }

  // Test 9: Julian day notation with extended hours
  {
    string_view spec = "EST5EDT,J100/167:00:00,J300/-167:00:00";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->dst_start.time.offset, 167 * 3600);
    EXPECT_EQ(result->dst_end.time.offset, -167 * 3600);
  }

  // Test 10: Zero-based day notation with extended hours
  {
    string_view spec = "EST5EDT,100/167:59:59,300/-167:59:59";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->dst_start.time.offset, 167 * 3600 + 59 * 60 + 59);
    EXPECT_EQ(result->dst_end.time.offset, -167 * 3600 - 59 * 60 - 59);
  }

  // Test 11: Positive extended hours without explicit plus sign
  {
    string_view spec = "EST5EDT,M3.2.0/100,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->dst_start.time.offset, 100 * 3600);
  }

  // Test 12: Edge case - exactly at boundaries (167 and -167)
  {
    string_view spec = "EST5EDT,M3.2.0/167,M11.1.0/-167";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->dst_start.time.offset, 167 * 3600);
    EXPECT_EQ(result->dst_end.time.offset, -167 * 3600);
  }
}

TEST(LlvmLibcParsePosixSpec, ColonPrefixRejected) {
  using LIBC_NAMESPACE::cpp::string_view;
  using LIBC_NAMESPACE::time_zone_posix::PosixTimeZone;

  // Colon-prefix is implementation-defined per POSIX.
  // Most Unix systems treat ":America/New_York" as a path to load IANA
  // timezone database files (tzfile format) from /usr/share/zoneinfo/.
  //
  // This implementation currently only supports POSIX TZ rules, not tzfile
  // loading, so colon-prefixed strings are rejected and return nullopt.
  // Future support for tzfile loading is planned (Phase 7).

  // Test 1: Typical IANA timezone path
  {
    string_view spec = ":America/New_York";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  // Test 2: Another common timezone path
  {
    string_view spec = ":US/Pacific";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  // Test 3: UTC path
  {
    string_view spec = ":UTC";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  // Test 4: Europe timezone path
  {
    string_view spec = ":Europe/London";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  // Test 5: Just a colon
  {
    string_view spec = ":";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  // Test 6: Colon with absolute path
  // NOTE: Absolute paths (e.g., ":/some/path") should NEVER be supported.
  // This would be a security vulnerability, especially for privileged
  // processes, as it could allow loading arbitrary files from the filesystem.
  // Standard IANA paths are relative (e.g., ":America/New_York" looks in
  // /usr/share/zoneinfo/America/New_York), never absolute.
  {
    string_view spec = ":/some/random/path";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }
}
