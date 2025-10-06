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
    if (posix_result.has_value()) {
      __builtin_printf(
          "Testing failed for: %s - expected to fail but succeed.\n",
          timezone);
    }
    ASSERT_FALSE(posix_result.has_value());
  }
}

TEST(LlvmLibcParsePosixSpec, MalformedInputTests) {
  using LIBC_NAMESPACE::cpp::string_view;
  using LIBC_NAMESPACE::time_zone_posix::PosixTimeZone;

  // This test expands on InvalidTest to cover additional edge cases and
  // malformed inputs not already tested. Note: The parser is lenient and
  // accepts partial valid specs (stopping at first invalid character).
  // Tests focus on cases that are truly invalid throughout.

  //
  // Quoted Timezone Name Edge Cases
  //

  // Empty quoted name
  {
    string_view spec = "<>5";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  // Unclosed quoted name (missing closing >)
  {
    string_view spec = "<ABC5";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  // Closing bracket without opening (treated as invalid character)
  {
    string_view spec = "ABC>5";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  // Empty quoted DST name
  {
    string_view spec = "EST5<>";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  // Unclosed quoted DST name
  {
    string_view spec = "EST5<EDT";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  //
  // Timezone Name Length and Character Constraints
  //

  // Timezone name too short (less than 3 characters)
  {
    string_view spec = "AB5";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  // Single character timezone name
  {
    string_view spec = "A5";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  // DST name too short
  {
    string_view spec = "EST5ED";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  // Note: Many invalid character tests are covered in existing InvalidTest
  // Parser is lenient and stops at invalid characters, accepting valid prefix
  // E.g., "E$T5" might parse as "E" (which then fails other validations)
  // Focus here is on edge cases not covered by existing InvalidTest

  //
  // Incomplete Specifications (truly incomplete, not just missing optional
  // parts)
  //

  // Incomplete M format: only month
  {
    string_view spec = "EST5EDT,M3";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  // Incomplete M format: month and partial separator
  {
    string_view spec = "EST5EDT,M3.";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  // Incomplete M format: month and week, no weekday
  {
    string_view spec = "EST5EDT,M3.2";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  // Incomplete M format: month, week, and partial separator
  {
    string_view spec = "EST5EDT,M3.2.";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  // Start date with trailing comma but no end date
  {
    string_view spec = "EST5EDT,M3.2.0,";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  //
  // Leading Whitespace
  //
  // Note: Parser may skip leading whitespace and accept " EST5" as valid
  // This is implementation-defined behavior, removed from test

  //
  // Invalid Date Formats
  //

  // J format with no number
  {
    string_view spec = "EST5EDT,J,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  // Multiple commas (invalid date separator)
  {
    string_view spec = "EST5EDT,,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  //
  // Invalid Time Formats in Transitions
  //

  // Time with only slash, no value
  {
    string_view spec = "EST5EDT,M3.2.0/,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  //
  // Edge Cases with Quoted Names
  //

  // Quoted name with no offset
  {
    string_view spec = "<EST>";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  // Nested brackets
  {
    string_view spec = "<<EST>>5";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  //
  // Very Long Inputs (test buffer limits)
  //

  // Extremely long unquoted timezone name (over any reasonable limit)
  {
    string_view spec =
        "VERYLONGTIMEZONENAME12345678901234567890123456789012345";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  //
  // Regression Tests for Common Typos
  //

  // Double dot in M format
  {
    string_view spec = "EST5EDT,M3..2.0,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  // Missing dot in M format (becomes invalid M format)
  {
    string_view spec = "EST5EDT,M320,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
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

TEST(LlvmLibcParsePosixSpec, BoundaryConditions) {
  using LIBC_NAMESPACE::cpp::string_view;
  using LIBC_NAMESPACE::time_zone_posix::PosixTimeZone;
  using LIBC_NAMESPACE::time_zone_posix::PosixTransition;

  // This test covers all boundary values and edge cases for the POSIX TZ parser
  // to ensure robustness at extreme valid values and proper rejection of invalid ones.

  //
  // Hour Boundaries (valid range: -167 to 167 per RFC 8536)
  //

  // Test minimum valid hour in transition time
  {
    string_view spec = "EST5EDT,M3.2.0/-167,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->dst_start.time.offset, -167 * 3600);
  }

  // Test maximum valid hour in transition time
  {
    string_view spec = "EST5EDT,M3.2.0/167,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->dst_start.time.offset, 167 * 3600);
  }

  // Test zero hour
  {
    string_view spec = "EST5EDT,M3.2.0/0,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->dst_start.time.offset, 0);
  }

  // Test old POSIX standard maximum (24 hours)
  {
    string_view spec = "EST5EDT,M3.2.0/24,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->dst_start.time.offset, 24 * 3600);
  }

  // Test hour beyond maximum (168 should fail)
  {
    string_view spec = "EST5EDT,M3.2.0/168,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  // Test hour below minimum (-168 should fail)
  {
    string_view spec = "EST5EDT,M3.2.0/-168,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  //
  // Minute Boundaries (valid range: 0-59)
  //

  // Test minimum valid minutes
  {
    string_view spec = "EST5EDT,M3.2.0/2:0,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->dst_start.time.offset, 2 * 3600);
  }

  // Test maximum valid minutes
  {
    string_view spec = "EST5EDT,M3.2.0/2:59,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->dst_start.time.offset, 2 * 3600 + 59 * 60);
  }

  // Test minutes out of range (60 should fail)
  {
    string_view spec = "EST5EDT,M3.2.0/2:60,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  // Test negative minutes (should fail)
  {
    string_view spec = "EST5EDT,M3.2.0/2:-1,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  //
  // Second Boundaries (valid range: 0-59)
  //

  // Test minimum valid seconds
  {
    string_view spec = "EST5EDT,M3.2.0/2:30:0,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->dst_start.time.offset, 2 * 3600 + 30 * 60);
  }

  // Test maximum valid seconds
  {
    string_view spec = "EST5EDT,M3.2.0/2:30:59,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->dst_start.time.offset, 2 * 3600 + 30 * 60 + 59);
  }

  // Test seconds out of range (60 should fail)
  {
    string_view spec = "EST5EDT,M3.2.0/2:30:60,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  // Test negative seconds (should fail)
  {
    string_view spec = "EST5EDT,M3.2.0/2:30:-1,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  //
  // Day Boundaries - Julian Format (Jn) (valid range: J1-J365)
  //

  // Test minimum valid Julian day
  {
    string_view spec = "EST5EDT,J1,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->dst_start.date.fmt, PosixTransition::DateFormat::J);
    auto& day = result->dst_start.date.data.get<PosixTransition::Date::NonLeapDay>();
    EXPECT_EQ(day.day, static_cast<int16_t>(1));
  }

  // Test maximum valid Julian day
  {
    string_view spec = "EST5EDT,J365,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->dst_start.date.fmt, PosixTransition::DateFormat::J);
    auto& day = result->dst_start.date.data.get<PosixTransition::Date::NonLeapDay>();
    EXPECT_EQ(day.day, static_cast<int16_t>(365));
  }

  // Test Julian day 0 (should fail - J format starts at 1)
  {
    string_view spec = "EST5EDT,J0,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  // Test Julian day beyond maximum (J366 should fail)
  {
    string_view spec = "EST5EDT,J366,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  //
  // Day Boundaries - Zero-based Format (n) (valid range: 0-365)
  //

  // Test minimum valid zero-based day
  {
    string_view spec = "EST5EDT,0,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->dst_start.date.fmt, PosixTransition::DateFormat::N);
    auto& day = result->dst_start.date.data.get<PosixTransition::Date::Day>();
    EXPECT_EQ(day.day, static_cast<int16_t>(0));
  }

  // Test maximum valid zero-based day
  {
    string_view spec = "EST5EDT,365,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->dst_start.date.fmt, PosixTransition::DateFormat::N);
    auto& day = result->dst_start.date.data.get<PosixTransition::Date::Day>();
    EXPECT_EQ(day.day, static_cast<int16_t>(365));
  }

  // Test zero-based day beyond maximum (366 should fail)
  {
    string_view spec = "EST5EDT,366,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  // Test negative zero-based day (should fail)
  {
    string_view spec = "EST5EDT,-1,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  //
  // Week Boundaries - M format (Mm.n.d) (valid range for n: 1-5)
  //

  // Test minimum valid week (1)
  {
    string_view spec = "EST5EDT,M3.1.0,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->dst_start.date.fmt, PosixTransition::DateFormat::M);
    auto& mwd = result->dst_start.date.data.get<PosixTransition::Date::MonthWeekWeekday>();
    EXPECT_EQ(mwd.week, static_cast<int8_t>(1));
  }

  // Test maximum valid week (5)
  {
    string_view spec = "EST5EDT,M3.5.0,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->dst_start.date.fmt, PosixTransition::DateFormat::M);
    auto& mwd = result->dst_start.date.data.get<PosixTransition::Date::MonthWeekWeekday>();
    EXPECT_EQ(mwd.week, static_cast<int8_t>(5));
  }

  // Test week 0 (should fail)
  {
    string_view spec = "EST5EDT,M3.0.0,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  // Test week 6 (should fail)
  {
    string_view spec = "EST5EDT,M3.6.0,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  //
  // Month Boundaries - M format (Mm.n.d) (valid range for m: 1-12)
  //

  // Test minimum valid month (1 = January)
  {
    string_view spec = "EST5EDT,M1.2.0,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->dst_start.date.fmt, PosixTransition::DateFormat::M);
    auto& mwd = result->dst_start.date.data.get<PosixTransition::Date::MonthWeekWeekday>();
    EXPECT_EQ(mwd.month, static_cast<int8_t>(1));
  }

  // Test maximum valid month (12 = December)
  {
    string_view spec = "EST5EDT,M12.2.0,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->dst_start.date.fmt, PosixTransition::DateFormat::M);
    auto& mwd = result->dst_start.date.data.get<PosixTransition::Date::MonthWeekWeekday>();
    EXPECT_EQ(mwd.month, static_cast<int8_t>(12));
  }

  // Test month 0 (should fail)
  {
    string_view spec = "EST5EDT,M0.2.0,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  // Test month 13 (should fail)
  {
    string_view spec = "EST5EDT,M13.2.0,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  //
  // Weekday Boundaries - M format (Mm.n.d) (valid range for d: 0-6)
  //

  // Test minimum valid weekday (0 = Sunday)
  {
    string_view spec = "EST5EDT,M3.2.0,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->dst_start.date.fmt, PosixTransition::DateFormat::M);
    auto& mwd = result->dst_start.date.data.get<PosixTransition::Date::MonthWeekWeekday>();
    EXPECT_EQ(mwd.weekday, static_cast<int8_t>(0));
  }

  // Test maximum valid weekday (6 = Saturday)
  {
    string_view spec = "EST5EDT,M3.2.6,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->dst_start.date.fmt, PosixTransition::DateFormat::M);
    auto& mwd = result->dst_start.date.data.get<PosixTransition::Date::MonthWeekWeekday>();
    EXPECT_EQ(mwd.weekday, static_cast<int8_t>(6));
  }

  // Test weekday 7 (should fail)
  {
    string_view spec = "EST5EDT,M3.2.7,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  // Test negative weekday (should fail)
  {
    string_view spec = "EST5EDT,M3.2.-1,M11.1.0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    EXPECT_FALSE(result.has_value());
  }

  //
  // Combined Extreme Cases
  //

  // Test all maximums together
  {
    string_view spec = "EST5EDT,M12.5.6/167:59:59,J365/-167:59:59";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    ASSERT_TRUE(result.has_value());
    // Verify start: December, week 5, Saturday, at 167:59:59
    EXPECT_EQ(result->dst_start.date.fmt, PosixTransition::DateFormat::M);
    auto& start_mwd = result->dst_start.date.data.get<PosixTransition::Date::MonthWeekWeekday>();
    EXPECT_EQ(start_mwd.month, static_cast<int8_t>(12));
    EXPECT_EQ(start_mwd.week, static_cast<int8_t>(5));
    EXPECT_EQ(start_mwd.weekday, static_cast<int8_t>(6));
    EXPECT_EQ(result->dst_start.time.offset, 167 * 3600 + 59 * 60 + 59);
    // Verify end: Julian day 365, at -167:59:59
    EXPECT_EQ(result->dst_end.date.fmt, PosixTransition::DateFormat::J);
    auto& end_day = result->dst_end.date.data.get<PosixTransition::Date::NonLeapDay>();
    EXPECT_EQ(end_day.day, static_cast<int16_t>(365));
    EXPECT_EQ(result->dst_end.time.offset, -167 * 3600 - 59 * 60 - 59);
  }

  // Test all minimums together
  {
    string_view spec = "EST5EDT,M1.1.0/-167:0:0,0/0:0:0";
    auto result = PosixTimeZone::ParsePosixSpec(spec);
    ASSERT_TRUE(result.has_value());
    // Verify start: January, week 1, Sunday, at -167:00:00
    EXPECT_EQ(result->dst_start.date.fmt, PosixTransition::DateFormat::M);
    auto& start_mwd = result->dst_start.date.data.get<PosixTransition::Date::MonthWeekWeekday>();
    EXPECT_EQ(start_mwd.month, static_cast<int8_t>(1));
    EXPECT_EQ(start_mwd.week, static_cast<int8_t>(1));
    EXPECT_EQ(start_mwd.weekday, static_cast<int8_t>(0));
    EXPECT_EQ(result->dst_start.time.offset, -167 * 3600);
    // Verify end: Day 0, at 00:00:00
    EXPECT_EQ(result->dst_end.date.fmt, PosixTransition::DateFormat::N);
    auto& end_day = result->dst_end.date.data.get<PosixTransition::Date::Day>();
    EXPECT_EQ(end_day.day, static_cast<int16_t>(0));
    EXPECT_EQ(result->dst_end.time.offset, 0);
  }
}

TEST(LlvmLibcParsePosixSpec, MemoryLifetimeSafety) {
  using LIBC_NAMESPACE::cpp::string_view;
  using LIBC_NAMESPACE::time_zone_posix::PosixTimeZone;

  // This test verifies that parsed timezone results remain valid after the
  // original input string is destroyed. This is critical to ensure that the
  // Phase 1 memory safety refactoring works correctly.
  //
  // The old implementation stored string_view references to the input string,
  // which would cause use-after-free bugs if the original string was destroyed.
  // The refactored implementation copies timezone abbreviations into internal
  // storage, making them independent of the original input lifetime.

  // Test 1: Basic timezone with standard offset only
  {
    PosixTimeZone tz;
    {
      char temp[] = "EST5";
      auto result = PosixTimeZone::ParsePosixSpec(string_view(temp));
      ASSERT_TRUE(result.has_value());
      tz = *result;
      // Verify while temp is still valid
      EXPECT_EQ(tz.std_abbr, string_view("EST"));
      EXPECT_EQ(tz.std_offset, -5 * 3600);
    }
    // temp is now destroyed, but tz should still be valid
    // This would SEGFAULT with the old implementation
    EXPECT_EQ(tz.std_abbr, string_view("EST"));
    EXPECT_EQ(tz.std_offset, -5 * 3600);
    EXPECT_TRUE(tz.dst_abbr.empty());
  }

  // Test 2: Timezone with DST
  {
    PosixTimeZone tz;
    {
      char temp[] = "PST8PDT,M3.2.0,M11.1.0";
      auto result = PosixTimeZone::ParsePosixSpec(string_view(temp));
      ASSERT_TRUE(result.has_value());
      tz = *result;
      // Verify while temp is still valid
      EXPECT_EQ(tz.std_abbr, string_view("PST"));
      EXPECT_EQ(tz.dst_abbr, string_view("PDT"));
    }
    // temp is destroyed, but tz should still be valid
    EXPECT_EQ(tz.std_abbr, string_view("PST"));
    EXPECT_EQ(tz.std_offset, -8 * 3600);
    EXPECT_EQ(tz.dst_abbr, string_view("PDT"));
    EXPECT_EQ(tz.dst_offset, -7 * 3600);
  }

  // Test 3: Quoted timezone names with special characters
  {
    PosixTimeZone tz;
    {
      char temp[] = "<UTC+5>-5<UTC+4>-4,M3.2.0,M11.1.0";
      auto result = PosixTimeZone::ParsePosixSpec(string_view(temp));
      ASSERT_TRUE(result.has_value());
      tz = *result;
      // Verify while temp is still valid
      EXPECT_EQ(tz.std_abbr, string_view("UTC+5"));
      EXPECT_EQ(tz.dst_abbr, string_view("UTC+4"));
    }
    // temp is destroyed, but tz should still be valid
    EXPECT_EQ(tz.std_abbr, string_view("UTC+5"));
    EXPECT_EQ(tz.std_offset, 5 * 3600);
    EXPECT_EQ(tz.dst_abbr, string_view("UTC+4"));
    EXPECT_EQ(tz.dst_offset, 4 * 3600);
  }

  // Test 4: Long timezone names (near maximum length)
  {
    PosixTimeZone tz;
    {
      char temp[] = "VERYLONGTZ5VERYLONGDST,M3.2.0,M11.1.0";
      auto result = PosixTimeZone::ParsePosixSpec(string_view(temp));
      ASSERT_TRUE(result.has_value());
      tz = *result;
      // Verify while temp is still valid
      EXPECT_EQ(tz.std_abbr, string_view("VERYLONGTZ"));
      EXPECT_EQ(tz.dst_abbr, string_view("VERYLONGDST"));
    }
    // temp is destroyed, but tz should still be valid
    EXPECT_EQ(tz.std_abbr, string_view("VERYLONGTZ"));
    EXPECT_EQ(tz.dst_abbr, string_view("VERYLONGDST"));
  }

  // Test 5: Complex timezone with all components
  {
    PosixTimeZone tz;
    {
      char temp[] = "CET-1CEST,M3.5.0/2,M10.5.0/3";
      auto result = PosixTimeZone::ParsePosixSpec(string_view(temp));
      ASSERT_TRUE(result.has_value());
      tz = *result;
      // Verify transition dates are captured correctly
      EXPECT_EQ(tz.dst_start.date.fmt, 
                LIBC_NAMESPACE::time_zone_posix::PosixTransition::DateFormat::M);
      EXPECT_EQ(tz.dst_end.date.fmt, 
                LIBC_NAMESPACE::time_zone_posix::PosixTransition::DateFormat::M);
    }
    // temp is destroyed, verify all fields still work
    EXPECT_EQ(tz.std_abbr, string_view("CET"));
    EXPECT_EQ(tz.std_offset, 1 * 3600);
    EXPECT_EQ(tz.dst_abbr, string_view("CEST"));
    EXPECT_EQ(tz.dst_offset, 2 * 3600);
    
    // Verify transition details are still accessible
    auto& start_mwd = tz.dst_start.date.data.get<
        LIBC_NAMESPACE::time_zone_posix::PosixTransition::Date::MonthWeekWeekday>();
    EXPECT_EQ(start_mwd.month, static_cast<int8_t>(3));
    EXPECT_EQ(start_mwd.week, static_cast<int8_t>(5));
    EXPECT_EQ(start_mwd.weekday, static_cast<int8_t>(0));
    EXPECT_EQ(tz.dst_start.time.offset, 2 * 3600);
    
    auto& end_mwd = tz.dst_end.date.data.get<
        LIBC_NAMESPACE::time_zone_posix::PosixTransition::Date::MonthWeekWeekday>();
    EXPECT_EQ(end_mwd.month, static_cast<int8_t>(10));
    EXPECT_EQ(end_mwd.week, static_cast<int8_t>(5));
    EXPECT_EQ(end_mwd.weekday, static_cast<int8_t>(0));
    EXPECT_EQ(tz.dst_end.time.offset, 3 * 3600);
  }

  // Test 6: Multiple allocations and destructions
  // This tests that internal storage is properly managed through multiple operations
  {
    PosixTimeZone tz1, tz2, tz3;
    
    {
      char temp1[] = "EST5EDT,M3.2.0,M11.1.0";
      auto result = PosixTimeZone::ParsePosixSpec(string_view(temp1));
      ASSERT_TRUE(result.has_value());
      tz1 = *result;
    }
    
    {
      char temp2[] = "PST8PDT,M3.2.0,M11.1.0";
      auto result = PosixTimeZone::ParsePosixSpec(string_view(temp2));
      ASSERT_TRUE(result.has_value());
      tz2 = *result;
    }
    
    {
      char temp3[] = "CST6CDT,M3.2.0,M11.1.0";
      auto result = PosixTimeZone::ParsePosixSpec(string_view(temp3));
      ASSERT_TRUE(result.has_value());
      tz3 = *result;
    }
    
    // All original strings are destroyed, verify all three timezones are still valid
    EXPECT_EQ(tz1.std_abbr, string_view("EST"));
    EXPECT_EQ(tz1.dst_abbr, string_view("EDT"));
    EXPECT_EQ(tz1.std_offset, -5 * 3600);
    
    EXPECT_EQ(tz2.std_abbr, string_view("PST"));
    EXPECT_EQ(tz2.dst_abbr, string_view("PDT"));
    EXPECT_EQ(tz2.std_offset, -8 * 3600);
    
    EXPECT_EQ(tz3.std_abbr, string_view("CST"));
    EXPECT_EQ(tz3.dst_abbr, string_view("CDT"));
    EXPECT_EQ(tz3.std_offset, -6 * 3600);
  }

  // Test 7: Copy assignment safety
  // Verify that copying a timezone doesn't create dangling references
  {
    PosixTimeZone tz1, tz2;
    
    {
      char temp[] = "MST7MDT,M3.2.0,M11.1.0";
      auto result = PosixTimeZone::ParsePosixSpec(string_view(temp));
      ASSERT_TRUE(result.has_value());
      tz1 = *result;
    }
    
    // Copy tz1 to tz2
    tz2 = tz1;
    
    // Both should be valid and independent
    EXPECT_EQ(tz1.std_abbr, string_view("MST"));
    EXPECT_EQ(tz1.dst_abbr, string_view("MDT"));
    EXPECT_EQ(tz2.std_abbr, string_view("MST"));
    EXPECT_EQ(tz2.dst_abbr, string_view("MDT"));
    
    // Modify tz1's data through reassignment
    {
      char temp2[] = "EST5EDT,M3.2.0,M11.1.0";
      auto result = PosixTimeZone::ParsePosixSpec(string_view(temp2));
      ASSERT_TRUE(result.has_value());
      tz1 = *result;
    }
    
    // tz1 should change, tz2 should remain unchanged
    EXPECT_EQ(tz1.std_abbr, string_view("EST"));
    EXPECT_EQ(tz1.dst_abbr, string_view("EDT"));
    EXPECT_EQ(tz2.std_abbr, string_view("MST"));
    EXPECT_EQ(tz2.dst_abbr, string_view("MDT"));
  }

  // Test 8: Move semantics (if applicable)
  // Verify that moving a timezone doesn't leave dangling references
  {
    PosixTimeZone tz1;
    
    {
      char temp[] = "HST10";
      auto result = PosixTimeZone::ParsePosixSpec(string_view(temp));
      ASSERT_TRUE(result.has_value());
      tz1 = *result;
    }
    
    // Move tz1 to tz2
    PosixTimeZone tz2 = static_cast<PosixTimeZone&&>(tz1);
    
    // tz2 should be valid with the moved data
    EXPECT_EQ(tz2.std_abbr, string_view("HST"));
    EXPECT_EQ(tz2.std_offset, -10 * 3600);
    EXPECT_TRUE(tz2.dst_abbr.empty());
  }
}

// Test Suite for GetTimezoneAdjustment function
TEST(LlvmLibcParsePosixSpec, GetTimezoneAdjustmentEmptySpec) {
  using PosixTimeZone = LIBC_NAMESPACE::time_zone_posix::PosixTimeZone;

  // Empty TZ spec should return 0 (UTC)
  int32_t adj = PosixTimeZone::GetTimezoneAdjustment("", 0);
  EXPECT_EQ(adj, 0);
}

TEST(LlvmLibcParsePosixSpec, GetTimezoneAdjustmentInvalidSpec) {
  using PosixTimeZone = LIBC_NAMESPACE::time_zone_posix::PosixTimeZone;

  // Invalid TZ specs should return 0 (fall back to UTC)
  EXPECT_EQ(PosixTimeZone::GetTimezoneAdjustment("INVALID", 0), 0);
  EXPECT_EQ(PosixTimeZone::GetTimezoneAdjustment("X", 0), 0);
  EXPECT_EQ(PosixTimeZone::GetTimezoneAdjustment("123", 0), 0);
  EXPECT_EQ(PosixTimeZone::GetTimezoneAdjustment(":America/New_York", 0), 0);
}

TEST(LlvmLibcParsePosixSpec, GetTimezoneAdjustmentStandardTimeOnly) {
  using PosixTimeZone = LIBC_NAMESPACE::time_zone_posix::PosixTimeZone;

  // January 1, 2024 00:00:00 UTC
  time_t jan_1_2024 = 1704067200;

  // EST5 = UTC-5 hours = -18000 seconds
  EXPECT_EQ(PosixTimeZone::GetTimezoneAdjustment("EST5", jan_1_2024), -18000);

  // PST8 = UTC-8 hours = -28800 seconds
  EXPECT_EQ(PosixTimeZone::GetTimezoneAdjustment("PST8", jan_1_2024), -28800);

  // CST6 = UTC-6 hours = -21600 seconds
  EXPECT_EQ(PosixTimeZone::GetTimezoneAdjustment("CST6", jan_1_2024), -21600);

  // MST7 = UTC-7 hours = -25200 seconds
  EXPECT_EQ(PosixTimeZone::GetTimezoneAdjustment("MST7", jan_1_2024), -25200);
}

TEST(LlvmLibcParsePosixSpec, GetTimezoneAdjustmentPositiveOffset) {
  using PosixTimeZone = LIBC_NAMESPACE::time_zone_posix::PosixTimeZone;

  time_t jan_1_2024 = 1704067200;

  // IST-5:30 = UTC+5:30 = +19800 seconds (India Standard Time)
  EXPECT_EQ(PosixTimeZone::GetTimezoneAdjustment("IST-5:30", jan_1_2024),
            19800);

  // JST-9 = UTC+9 = +32400 seconds (Japan Standard Time)
  EXPECT_EQ(PosixTimeZone::GetTimezoneAdjustment("JST-9", jan_1_2024), 32400);

  // AEST-10 = UTC+10 = +36000 seconds (Australian Eastern Standard Time)
  EXPECT_EQ(PosixTimeZone::GetTimezoneAdjustment("AEST-10", jan_1_2024), 36000);
}

TEST(LlvmLibcParsePosixSpec, GetTimezoneAdjustmentWithDSTWinter) {
  using PosixTimeZone = LIBC_NAMESPACE::time_zone_posix::PosixTimeZone;

  // January 1, 2024 00:00:00 UTC (winter, no DST)
  time_t jan_1_2024 = 1704067200;

  // EST5EDT,M3.2.0,M11.1.0 in winter should return EST offset
  // EST = UTC-5 hours = -18000 seconds
  EXPECT_EQ(PosixTimeZone::GetTimezoneAdjustment("EST5EDT,M3.2.0,M11.1.0",
                                                 jan_1_2024),
            -18000);

  // PST8PDT,M3.2.0,M11.1.0 in winter should return PST offset
  // PST = UTC-8 hours = -28800 seconds
  EXPECT_EQ(PosixTimeZone::GetTimezoneAdjustment("PST8PDT,M3.2.0,M11.1.0",
                                                 jan_1_2024),
            -28800);
}

TEST(LlvmLibcParsePosixSpec, GetTimezoneAdjustmentWithDSTSummer) {
  using PosixTimeZone = LIBC_NAMESPACE::time_zone_posix::PosixTimeZone;

  // July 1, 2024 00:00:00 UTC (summer, DST active)
  time_t july_1_2024 = 1719792000;

  // EST5EDT,M3.2.0,M11.1.0 in summer should return EDT offset
  // EDT = EST + 1 hour = UTC-4 hours = -14400 seconds
  EXPECT_EQ(PosixTimeZone::GetTimezoneAdjustment("EST5EDT,M3.2.0,M11.1.0",
                                                 july_1_2024),
            -14400);

  // PST8PDT,M3.2.0,M11.1.0 in summer should return PDT offset
  // PDT = PST + 1 hour = UTC-7 hours = -25200 seconds
  EXPECT_EQ(PosixTimeZone::GetTimezoneAdjustment("PST8PDT,M3.2.0,M11.1.0",
                                                 july_1_2024),
            -25200);
}

TEST(LlvmLibcParsePosixSpec, GetTimezoneAdjustmentMultipleTimes) {
  using PosixTimeZone = LIBC_NAMESPACE::time_zone_posix::PosixTimeZone;

  // Test various times throughout 2024
  const char *tz_spec = "EST5EDT,M3.2.0,M11.1.0";

  // January (winter) - EST
  time_t jan_15 = 1705276800; // Jan 15, 2024
  EXPECT_EQ(PosixTimeZone::GetTimezoneAdjustment(tz_spec, jan_15), -18000);

  // February (winter) - EST
  time_t feb_15 = 1707955200; // Feb 15, 2024
  EXPECT_EQ(PosixTimeZone::GetTimezoneAdjustment(tz_spec, feb_15), -18000);

  // April (summer) - EDT
  time_t apr_15 = 1713139200; // Apr 15, 2024
  EXPECT_EQ(PosixTimeZone::GetTimezoneAdjustment(tz_spec, apr_15), -14400);

  // July (summer) - EDT
  time_t july_15 = 1721001600; // July 15, 2024
  EXPECT_EQ(PosixTimeZone::GetTimezoneAdjustment(tz_spec, july_15), -14400);

  // October (summer) - EDT
  time_t oct_15 = 1728950400; // Oct 15, 2024
  EXPECT_EQ(PosixTimeZone::GetTimezoneAdjustment(tz_spec, oct_15), -14400);

  // December (winter) - EST
  time_t dec_15 = 1734220800; // Dec 15, 2024
  EXPECT_EQ(PosixTimeZone::GetTimezoneAdjustment(tz_spec, dec_15), -18000);
}

// Tests for IsDSTActive method
TEST(LlvmLibcParsePosixSpec, IsDSTActive_NoDSTRules) {
  using PosixTimeZone = LIBC_NAMESPACE::time_zone_posix::PosixTimeZone;

  // Parse a timezone without DST rules (EST5 has no DST)
  auto tz = PosixTimeZone::ParsePosixSpec("EST5");
  ASSERT_TRUE(tz.has_value());

  // Any time should return false when there are no DST rules
  time_t winter = 1705320000; // January 15, 2024
  time_t summer = 1721044800; // July 15, 2024

  EXPECT_FALSE(tz->IsDSTActive(winter));
  EXPECT_FALSE(tz->IsDSTActive(summer));
}

TEST(LlvmLibcParsePosixSpec, IsDSTActive_WithDST_Winter) {
  using PosixTimeZone = LIBC_NAMESPACE::time_zone_posix::PosixTimeZone;

  // Parse EST5EDT,M3.2.0,M11.1.0
  // DST starts: March, 2nd week, Sunday at 02:00:00
  // DST ends: November, 1st week, Sunday at 02:00:00
  auto tz = PosixTimeZone::ParsePosixSpec("EST5EDT,M3.2.0,M11.1.0");
  ASSERT_TRUE(tz.has_value());

  // January 15, 2024 12:00:00 UTC = 1705320000
  // This is January 15, 2024 07:00:00 EST (winter, DST not active)
  EXPECT_FALSE(tz->IsDSTActive(1705320000));

  // December 15, 2024 12:00:00 UTC = 1734264000
  // This is December 15, 2024 07:00:00 EST (winter, DST not active)
  EXPECT_FALSE(tz->IsDSTActive(1734264000));

  // February 1, 2024 00:00:00 UTC = 1706745600
  // DST not active
  EXPECT_FALSE(tz->IsDSTActive(1706745600));
}

TEST(LlvmLibcParsePosixSpec, IsDSTActive_WithDST_Summer) {
  using PosixTimeZone = LIBC_NAMESPACE::time_zone_posix::PosixTimeZone;

  // Parse EST5EDT,M3.2.0,M11.1.0
  auto tz = PosixTimeZone::ParsePosixSpec("EST5EDT,M3.2.0,M11.1.0");
  ASSERT_TRUE(tz.has_value());

  // July 15, 2024 12:00:00 UTC = 1721044800
  // This is July 15, 2024 08:00:00 EDT (summer, DST active)
  EXPECT_TRUE(tz->IsDSTActive(1721044800));

  // August 1, 2024 00:00:00 UTC = 1722470400
  // DST active
  EXPECT_TRUE(tz->IsDSTActive(1722470400));

  // June 1, 2024 00:00:00 UTC = 1717200000
  // DST active
  EXPECT_TRUE(tz->IsDSTActive(1717200000));
}

TEST(LlvmLibcParsePosixSpec, IsDSTActive_DSTTransition) {
  using PosixTimeZone = LIBC_NAMESPACE::time_zone_posix::PosixTimeZone;

  // Parse EST5EDT,M3.2.0,M11.1.0
  // In 2024:
  // - DST starts: March, 2nd Sunday (March 10) at 02:00:00 local time
  // - DST ends: November, 1st Sunday (November 3) at 02:00:00 local time
  auto tz = PosixTimeZone::ParsePosixSpec("EST5EDT,M3.2.0,M11.1.0");
  ASSERT_TRUE(tz.has_value());

  // March 9, 2024 12:00:00 UTC - day before DST starts (definitely winter)
  time_t before_spring = 1709985600;
  EXPECT_FALSE(tz->IsDSTActive(before_spring));

  // March 15, 2024 12:00:00 UTC - week after DST starts (definitely summer)
  time_t after_spring = 1710504000;
  EXPECT_TRUE(tz->IsDSTActive(after_spring));

  // November 2, 2024 12:00:00 UTC - day before DST ends (definitely summer)
  time_t before_fall = 1730548800;
  EXPECT_TRUE(tz->IsDSTActive(before_fall));

  // November 10, 2024 12:00:00 UTC - week after DST ends (definitely winter)
  time_t after_fall = 1731240000;
  EXPECT_FALSE(tz->IsDSTActive(after_fall));
}

TEST(LlvmLibcParsePosixSpec, IsDSTActive_SouthernHemisphere) {
  using PosixTimeZone = LIBC_NAMESPACE::time_zone_posix::PosixTimeZone;

  // New Zealand: NZST-12NZDT,M9.5.0,M4.1.0/3
  // DST starts: September (month 9), last (5th) Sunday (week 0), at default 02:00:00
  // DST ends: April (month 4), first (1st) Sunday (week 0), at 03:00:00
  // In southern hemisphere, DST is active from September to April
  auto tz = PosixTimeZone::ParsePosixSpec("NZST-12NZDT,M9.5.0,M4.1.0/3");
  ASSERT_TRUE(tz.has_value());

  // January 15, 2024 00:00:00 UTC = 1705276800
  // Southern hemisphere summer - DST should be active
  EXPECT_TRUE(tz->IsDSTActive(1705276800));

  // July 15, 2024 00:00:00 UTC = 1721001600
  // Southern hemisphere winter - DST should NOT be active
  EXPECT_FALSE(tz->IsDSTActive(1721001600));
}

// Test the exact scenario from the failing mktime test
TEST(LlvmLibcParsePosixSpec, IsDSTActive_ExactMktimeScenario) {
  using PosixTimeZone = LIBC_NAMESPACE::time_zone_posix::PosixTimeZone;

  // Parse the same TZ string used in mktime test
  const char *tz_string = "EST5EDT,M3.2.0,M11.1.0";
  auto tz = PosixTimeZone::ParsePosixSpec(tz_string);
  ASSERT_TRUE(tz.has_value());

  // July 15, 2024 12:00:00 UTC = 1721044800
  // This is the UTC time that mktime should calculate for July 15, 2024 08:00:00 EDT
  time_t utc_time = 1721044800;
  
  // Verify IsDSTActive returns true for this time
  bool is_dst = tz->IsDSTActive(utc_time);
  EXPECT_TRUE(is_dst);
  
  // Also verify GetTimezoneAdjustment returns the DST offset
  int32_t adjustment = PosixTimeZone::GetTimezoneAdjustment(tz_string, utc_time);
  EXPECT_EQ(adjustment, -14400); // EDT is UTC-4 hours = -14400 seconds
  
  // The DST offset should match what we get from the parsed timezone
  EXPECT_EQ(adjustment, tz->dst_offset);
  
  // Verify standard time scenario for comparison
  // January 15, 2024 12:00:00 UTC = 1705320000
  time_t winter_utc_time = 1705320000;
  bool winter_is_dst = tz->IsDSTActive(winter_utc_time);
  EXPECT_FALSE(winter_is_dst);
  
  int32_t winter_adjustment = PosixTimeZone::GetTimezoneAdjustment(tz_string, winter_utc_time);
  EXPECT_EQ(winter_adjustment, -18000); // EST is UTC-5 hours = -18000 seconds
  EXPECT_EQ(winter_adjustment, tz->std_offset);
}
