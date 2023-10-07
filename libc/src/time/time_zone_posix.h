//===-- Collection of utils for TZ parsing ----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC_TIME_TIME_ZONE_POSIX_H
#define LLVM_LIBC_SRC_TIME_TIME_ZONE_POSIX_H

#include <stdint.h> // int8_t, int16_t and int32_t

#include "src/__support/CPP/optional.h"
#include "src/__support/CPP/string_view.h"

namespace LIBC_NAMESPACE {
namespace time_zone_posix {

// This enum is used to handle + or - symbol in the offset specification of
// TZ, which is of the fromat "[+|-]hh[:mm[:ss]]".
// If specification says +, then use the TZOffset as is.
// If specification says -, then reverse the TZOffset,
// if TZOffset is -1, then use +1 otherwise use +1 as the multiplier.
//
// TZOffset is used in numeric calculations, thus we couldn't define it as enum
// class.
enum TZOffset { NEGATIVE = -1, POSITIVE = 1 };

// The TZ environment variable is specified in
// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap08.html
//
// The following is an example of how the TZ spec is parsed and saved in the
// PosixTimeZone object.
//
// The current POSIX spec for America/Los_Angeles is "PST8PDT,M3.2.0,M11.1.0",
// which would be broken down as ...
//
//   PosixTimeZone {
//     std_abbr = "PST"
//     std_offset = -28800
//     dst_abbr = "PDT"
//     dst_offset = -25200
//     dst_start = PosixTransition {
//       date {
//         m {
//           month = 3
//           week = 2
//           weekday = 0
//         }
//       }
//       time {
//         offset = 7200
//       }
//     }
//     dst_end = PosixTransition {
//       date {
//         m {
//           month = 11
//           week = 1
//           weekday = 0
//         }
//       }
//       time {
//         offset = 7200
//       }
//     }
//   }
//
// The date/time of the transition. The date is specified as either:
// (J) the Nth day of the year (1 <= N <= 365), excluding leap days, or
// (N) the Nth day of the year (0 <= N <= 365), including leap days, or
// (M) the Nth weekday of a month (e.g., the 2nd Sunday in March).
// The time, specified as a day offset, identifies the particular moment
// of the transition, and may be negative or >= 24h, and in which case
// it would take us to another day, and perhaps week, or even month.

class PosixTransition {
public:
  enum class DateFormat : int { J, N, M };

  struct Date {
    struct NonLeapDay {
      int16_t day; // day of non-leap year [1:365]
    };
    struct Day {
      int16_t day; // day of year [0:365]
    };
    struct MonthWeekWeekday {
      int8_t month;   // month of year [1:12]
      int8_t week;    // week of month [1:5] (5==last)
      int8_t weekday; // 0==Sun, ..., 6=Sat
    };

    DateFormat fmt;

    // TODO(rtenneti): convert the following to libc variant.
    union {
      NonLeapDay j;
      Day n;
      MonthWeekWeekday m;
    };
  };

  struct Time {
    int32_t offset; // seconds before/after 00:00:00
  };

  Date date;
  Time time;

  PosixTransition() {
    date.fmt = DateFormat::N;
    date.j.day = 0;
    time.offset = 0;
  }

  explicit PosixTransition(DateFormat fmt, int16_t day, int32_t offset) {
    date.fmt = fmt;
    date.j.day = day;
    time.offset = offset;
  }

  explicit PosixTransition(DateFormat fmt, int8_t month, int8_t week,
                           int8_t weekday, int32_t offset) {
    date.fmt = fmt;
    date.m.month = month;
    date.m.week = week;
    date.m.weekday = weekday;
    time.offset = offset;
  }
};

// The entirety of a POSIX-string specified time-zone rule. The standard
// abbreviation and offset are always given. If the time zone includes
// daylight saving, then the daylight abbreviation is non-empty and the
// remaining fields are also valid. Note that the start/end transitions
// are not ordered---in the southern hemisphere the transition to end
// daylight time occurs first in any particular year.
class PosixTimeZone {
public:
  // Default constructor for testing.
  PosixTimeZone()
      : spec(""), std_abbr("UTC"), std_offset(0), dst_abbr(""), dst_offset(0) {}

  explicit PosixTimeZone(const cpp::string_view &spec)
      : spec(spec), std_abbr("UTC"), std_offset(0), dst_abbr(""),
        dst_offset(0) {}

  explicit PosixTimeZone(const cpp::string_view &spec,
                         cpp::string_view std_abbr, int32_t std_offset,
                         cpp::string_view dst_abbr, int32_t dst_offset,
                         time_zone_posix::PosixTransition dst_start,
                         time_zone_posix::PosixTransition dst_end)
      : spec(spec), std_abbr(std_abbr), std_offset(std_offset),
        dst_abbr(dst_abbr), dst_offset(dst_offset), dst_start(dst_start),
        dst_end(dst_end) {}

  // Breaks down a POSIX time-zone specification into its constituent pieces,
  // filling in any missing values (DST offset, or start/end transition times)
  // with the standard-defined defaults. Returns false if the specification
  // could not be parsed (although some fields of *res may have been altered).
  static cpp::optional<PosixTimeZone>
  ParsePosixSpec(const cpp::string_view spec);

  cpp::string_view spec;

  cpp::string_view std_abbr;
  int32_t std_offset;

  cpp::string_view dst_abbr;
  int32_t dst_offset;

  PosixTransition dst_start;
  PosixTransition dst_end;

private:
  bool UpdateStdAbbr();
  bool UpdateDstAbbr();
  bool UpdateStdOffset();
  bool UpdateDstOffset();
  bool UpdateDstStart();
  bool UpdateDstEnd();
  bool SpecHasData();

  cpp::optional<int> ParseInt(int min, int max);
  cpp::optional<cpp::string_view> ParseAbbr();
  cpp::optional<int32_t> ParseOffset(int min_hour, int max_hour,
                                     TZOffset multiplier);
  cpp::optional<PosixTransition> ParseMonthWeekWeekday();
  cpp::optional<PosixTransition> ParseNonLeapDay();
  cpp::optional<PosixTransition> ParseLeapDay();
  cpp::optional<PosixTransition> ParseDateTime();
};

} // namespace time_zone_posix
} // namespace LIBC_NAMESPACE

#endif // LLVM_LIBC_SRC_TIME_TIME_ZONE_POSIX_H
