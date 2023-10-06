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

namespace __llvm_libc {
namespace tzmatcher {
namespace testing {

class PosixTimeZoneMatcher
    : public __llvm_libc::testing::Matcher<time_zone_posix::PosixTimeZone> {
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
              expectedValue.date.j.day == actualValue.date.j.day);
    case time_zone_posix::PosixTransition::DateFormat::N:
      return (expectedValue.date.fmt == actualValue.date.fmt &&
              expectedValue.time.offset == actualValue.time.offset &&
              expectedValue.date.n.day == actualValue.date.n.day);
    case time_zone_posix::PosixTransition::DateFormat::M:
      return (expectedValue.date.fmt == actualValue.date.fmt &&
              expectedValue.time.offset == actualValue.time.offset &&
              expectedValue.date.m.month == actualValue.date.m.month &&
              expectedValue.date.m.week == actualValue.date.m.week &&
              expectedValue.date.m.weekday == actualValue.date.m.weekday);
    }
  }

  void posix_transition_describe_value(
      const char *label, time_zone_posix::PosixTransition value,
      __llvm_libc::testutils::StreamWrapper &stream) {
    switch (value.date.fmt) {
    case time_zone_posix::PosixTransition::DateFormat::J:
      stream << label << ".date.fmt: J ";
      stream << label << ".date.j.day: " << value.date.j.day;
      break;
    case time_zone_posix::PosixTransition::DateFormat::N:
      stream << label << ".date.fmt: N ";
      stream << label << ".date.n.day: " << value.date.n.day;
      break;
    case time_zone_posix::PosixTransition::DateFormat::M:
      stream << label << ".date.fmt: M ";
      stream << label
             << ".date.m.month: " << static_cast<int>(value.date.m.month);
      stream << label
             << ".date.m.week: " << static_cast<int>(value.date.m.week);
      stream << label
             << ".date.m.weekday: " << static_cast<int>(value.date.m.weekday);
      break;
    }
    stream << label << ".time.offset: " << value.time.offset;
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

  void describeValue(const char *label, time_zone_posix::PosixTimeZone value,
                     __llvm_libc::testutils::StreamWrapper &stream) {
    stream << label;
    if (value.spec.data())
      stream << " m_spec: " << value.spec.data();
    else
      stream << " m_spec: null";
    stream << " m_std_abbr: " << value.std_abbr.data();
    stream << " m_std_offset: " << value.std_offset;
    stream << " m_dst_abbr: " << value.dst_abbr.data();
    stream << " m_dst_offset: " << value.dst_offset;
    posix_transition_describe_value(" m_dst_start", value.dst_start, stream);
    posix_transition_describe_value(" m_dst_end", value.dst_end, stream);
    stream << '\n';
  }

  void explainError(__llvm_libc::testutils::StreamWrapper &stream) override {
    describeValue("Expected PosixTimeZone value: ", expected, stream);
    describeValue("  Actual PosixTimeZone value: ", actual, stream);
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
    : public __llvm_libc::testing::Matcher<time_zone_posix::PosixTimeZone> {
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
                             struct PosixTimeZoneTestData value,
                             __llvm_libc::testutils::StreamWrapper &stream) {
    stream << label;
    stream << " m_spec: " << value.spec;
    stream << " m_std_abbr: " << value.std_abbr;
    stream << " m_std_offset: " << value.std_offset;
    stream << " m_dst_abbr: " << value.dst_abbr;
    stream << " m_dst_offset: " << value.dst_offset;
    stream << '\n';
  }

  void describeValue(const char *label, time_zone_posix::PosixTimeZone value,
                     __llvm_libc::testutils::StreamWrapper &stream) {
    stream << label;
    if (value.spec.data())
      stream << " m_spec: " << value.spec.data();
    else
      stream << " m_spec: null";
    stream << " m_std_abbr: " << value.std_abbr.data();
    stream << " m_std_offset: " << value.std_offset;
    stream << " m_dst_abbr: " << value.dst_abbr.data();
    stream << " m_dst_offset: " << value.dst_offset;
    stream << '\n';
  }

  void explainError(__llvm_libc::testutils::StreamWrapper &stream) override {
    describeExpectedValue("Expected PosixTimeZone value: ", expected, stream);
    describeValue("  Actual PosixTimeZone value: ", actual, stream);
  }
};

} // namespace testing
} // namespace tzmatcher
} // namespace __llvm_libc

#define EXPECT_POSIX_TIME_ZONE_EQ(expected, actual)                            \
  EXPECT_THAT((actual), __llvm_libc::tzmatcher::testing::PosixTimeZoneMatcher( \
                            (expected)))

#define EXPECT_POSIX_TIME_ZONE_TEST_DATA_EQ(expected, actual)                  \
  EXPECT_THAT((actual),                                                        \
              __llvm_libc::tzmatcher::testing::PosixTimeZoneTestDataMatcher(   \
                  (expected)))

#endif // LLVM_LIBC_TEST_SRC_TIME_TZ_MATCHER_H
