//===---- TZMatcher.h -------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TEST_SRC_TIME_TZ_MATCHER_H
#define LLVM_LIBC_TEST_SRC_TIME_TZ_MATCHER_H

#include "src/time/time_zone_posix.h"
#include "test/UnitTest/Test.h"

namespace LIBC_NAMESPACE {
namespace testing {

class PosixTimeZoneMatcher
    : public Matcher<time_zone_posix::PosixTimeZone> {
  time_zone_posix::PosixTimeZone expected;
  time_zone_posix::PosixTimeZone actual;

public:
  PosixTimeZoneMatcher(time_zone_posix::PosixTimeZone expectedValue)
      : expected(expectedValue) {}

  bool posix_transition_equals(time_zone_posix::PosixTransition expectedValue,
                               time_zone_posix::PosixTransition actualValue) {
    switch (expectedValue.date.fmt) {
    case time_zone_posix::PosixTransition::DateFormat::J:
      return (expectedValue.date.fmt == actualValue.date.fmt &&
              expectedValue.time.offset == actualValue.time.offset &&
              expectedValue.date.data.get<time_zone_posix::PosixTransition::Date::NonLeapDay>().day == 
              actualValue.date.data.get<time_zone_posix::PosixTransition::Date::NonLeapDay>().day);
    case time_zone_posix::PosixTransition::DateFormat::N:
      return (expectedValue.date.fmt == actualValue.date.fmt &&
              expectedValue.time.offset == actualValue.time.offset &&
              expectedValue.date.data.get<time_zone_posix::PosixTransition::Date::Day>().day == 
              actualValue.date.data.get<time_zone_posix::PosixTransition::Date::Day>().day);
    case time_zone_posix::PosixTransition::DateFormat::M:
      return (expectedValue.date.fmt == actualValue.date.fmt &&
              expectedValue.time.offset == actualValue.time.offset &&
              expectedValue.date.data.get<time_zone_posix::PosixTransition::Date::MonthWeekWeekday>().month == 
              actualValue.date.data.get<time_zone_posix::PosixTransition::Date::MonthWeekWeekday>().month &&
              expectedValue.date.data.get<time_zone_posix::PosixTransition::Date::MonthWeekWeekday>().week == 
              actualValue.date.data.get<time_zone_posix::PosixTransition::Date::MonthWeekWeekday>().week &&
              expectedValue.date.data.get<time_zone_posix::PosixTransition::Date::MonthWeekWeekday>().weekday == 
              actualValue.date.data.get<time_zone_posix::PosixTransition::Date::MonthWeekWeekday>().weekday);
    }
  }

  void posix_transition_describe_value(
      const char *label, time_zone_posix::PosixTransition value) {
    switch (value.date.fmt) {
    case time_zone_posix::PosixTransition::DateFormat::J:
      tlog << label << ".date.fmt: J ";
      tlog << label << ".date.j.day: " << 
        value.date.data.get<time_zone_posix::PosixTransition::Date::NonLeapDay>().day;
      break;
    case time_zone_posix::PosixTransition::DateFormat::N:
      tlog << label << ".date.fmt: N ";
      tlog << label << ".date.n.day: " << 
        value.date.data.get<time_zone_posix::PosixTransition::Date::Day>().day;
      break;
    case time_zone_posix::PosixTransition::DateFormat::M:
      tlog << label << ".date.fmt: M ";
      tlog << label
             << ".date.m.month: " << static_cast<int>(
               value.date.data.get<time_zone_posix::PosixTransition::Date::MonthWeekWeekday>().month);
      tlog << label
             << ".date.m.week: " << static_cast<int>(
               value.date.data.get<time_zone_posix::PosixTransition::Date::MonthWeekWeekday>().week);
      tlog << label
             << ".date.m.weekday: " << static_cast<int>(
               value.date.data.get<time_zone_posix::PosixTransition::Date::MonthWeekWeekday>().weekday);
      break;
    }
    tlog << label << ".time.offset: " << value.time.offset;
  }

  bool match(time_zone_posix::PosixTimeZone actualValue) {
    actual = actualValue;
    return (actual.spec == expected.spec &&
            actual.std_abbr == expected.std_abbr &&
            actual.std_offset == expected.std_offset &&
            actual.dst_abbr == expected.dst_abbr &&
            actual.dst_offset == expected.dst_offset &&
            posix_transition_equals(expected.dst_start, actual.dst_start) &&
            posix_transition_equals(expected.dst_end, actual.dst_end));
  }

  void describeValue(const char *label, time_zone_posix::PosixTimeZone value) {
    tlog << label;
    if (value.spec.data())
      tlog << " m_spec: " << value.spec.data();
    else
      tlog << " m_spec: null";
    tlog << " m_std_abbr: " << value.std_abbr.data();
    tlog << " m_std_offset: " << value.std_offset;
    tlog << " m_dst_abbr: " << value.dst_abbr.data();
    tlog << " m_dst_offset: " << value.dst_offset;
    posix_transition_describe_value(" m_dst_start", value.dst_start);
    posix_transition_describe_value(" m_dst_end", value.dst_end);
    tlog << '\n';
  }

  void explainError() override {
    describeValue("Expected PosixTimeZone value: ", expected);
    describeValue("  Actual PosixTimeZone value: ", actual);
  }
};

struct PosixTimeZoneTestData {
  const char *spec;
  const char *std_abbr;
  int std_offset;
  const char *dst_abbr;
  int dst_offset;
};

class PosixTimeZoneTestDataMatcher
    : public Matcher<time_zone_posix::PosixTimeZone> {
  struct PosixTimeZoneTestData expected;
  time_zone_posix::PosixTimeZone actual;

public:
  PosixTimeZoneTestDataMatcher(struct PosixTimeZoneTestData expectedValue)
      : expected(expectedValue) {}

  bool match(time_zone_posix::PosixTimeZone actualValue) {
    actual = actualValue;
    return (actual.std_abbr.starts_with(expected.std_abbr) &&
            actual.std_offset == expected.std_offset &&
            actual.dst_abbr.starts_with(expected.dst_abbr) &&
            actual.dst_offset == expected.dst_offset);
  }

  void describeExpectedValue(const char *label,
                             struct PosixTimeZoneTestData value) {
    tlog << label;
    tlog << " m_spec: " << value.spec;
    tlog << " m_std_abbr: " << value.std_abbr;
    tlog << " m_std_offset: " << value.std_offset;
    tlog << " m_dst_abbr: " << value.dst_abbr;
    tlog << " m_dst_offset: " << value.dst_offset;
    tlog << '\n';
  }

  void describeValue(const char *label, time_zone_posix::PosixTimeZone value) {
    tlog << label;
    if (value.spec.data())
      tlog << " m_spec: " << value.spec.data();
    else
      tlog << " m_spec: null";
    tlog << " m_std_abbr: " << value.std_abbr.data();
    tlog << " m_std_offset: " << value.std_offset;
    tlog << " m_dst_abbr: " << value.dst_abbr.data();
    tlog << " m_dst_offset: " << value.dst_offset;
    tlog << '\n';
  }

  void explainError() override {
    describeExpectedValue("Expected PosixTimeZone value: ", expected);
    describeValue("  Actual PosixTimeZone value: ", actual);
  }
};

} // namespace testing
} // namespace LIBC_NAMESPACE

#define EXPECT_POSIX_TIME_ZONE_EQ(expected, actual)                            \
  EXPECT_THAT((actual), LIBC_NAMESPACE::testing::PosixTimeZoneMatcher( \
                            (expected)))

#define EXPECT_POSIX_TIME_ZONE_TEST_DATA_EQ(expected, actual)                  \
  EXPECT_THAT((actual),                                                        \
              LIBC_NAMESPACE::testing::PosixTimeZoneTestDataMatcher(   \
                  (expected)))

#endif // LLVM_LIBC_TEST_SRC_TIME_TZ_MATCHER_H
