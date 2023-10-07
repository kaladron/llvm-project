//===-- Collection of utils for TZ parsing ----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include <stddef.h> // size_t

#include "src/ctype/isdigit.h"
#include "src/stdlib/strtol.h"
#include "src/string/strchr.h"
#include "src/time/time_utils.h"
#include "src/time/time_zone_posix.h"

namespace LIBC_NAMESPACE {
namespace time_zone_posix {

using LIBC_NAMESPACE::time_utils::TimeConstants;

// https://datatracker.ietf.org/doc/html/rfc8536
// The hours part of the transition times may be signed and range
// from -167 through 167 (-167 <= hh <= 167) instead of the POSIX-
// required unsigned values from 0 through 24.
static constexpr int MAX_HOURS_IN_TRANSITION_TIMES = 167;

// TOOD(rtenneti): Make ParseInt be a template and return int8_t,
// unit16_t, etc? That way the static_casts below wouldn't be needed.
//
// Parses int value from the spec. It returns nullopt if the value
// is less than min or greater than max.
cpp::optional<int> PosixTimeZone::ParseInt(int min, int max) {
  if (spec.empty())
    return cpp::nullopt;

  char *pEnd;
  int value = strtol(spec.data(), &pEnd, 10); // NOLINT(runtime/deprecated_fn)
  if (value < min || value > max)
    return cpp::nullopt;

  // Delete the number from the input string.
  size_t len = pEnd - spec.data();
  spec.remove_prefix(len);
  return value;
}

// abbr = <.*?> | [^-+,\d]{3,}
cpp::optional<cpp::string_view> PosixTimeZone::ParseAbbr() {
  if (spec.empty())
    return cpp::nullopt;

  // Handle special zoneinfo <...> form.
  cpp::optional<cpp::string_view> abbr;
  if (spec.starts_with('<')) {
    spec.remove_prefix(1);
    const auto pos = spec.find_first_of('>');
    if (pos == cpp::string_view::npos)
      return cpp::nullopt;
    abbr = spec.substr(0, pos);
    // Delete the data up to and including '>' character.
    spec.remove_prefix(pos + 1);
    return abbr;
  }

  size_t len = 0;
  // Handle [^-+,\d]{3,}
  for (const auto &p : spec) {
    if (strchr("-+,", p))
      break;
    if (isdigit(p))
      break;
    len++;
  }
  if (len < 3)
    return cpp::nullopt;
  abbr = spec.substr(0, len);
  spec.remove_prefix(len);
  return abbr;
}

// offset = [+|-]hh[:mm[:ss]] (aggregated into single seconds value)
// default_sign_for_offset for std and dst offset is negative. For example, PST
// and PDT have default minus sign for the offset (relative to UTC). In,
// PST8PDT, though 8 doesn't have any sign, but the offset is (-8 * 3600).
cpp::optional<int32_t>
PosixTimeZone::ParseOffset(int min_hour, int max_hour,
                           TZOffset default_sign_for_offset) {
  if (spec.empty())
    return cpp::nullopt;

  // Handle [+|-].
  int multiplier = default_sign_for_offset;
  if (spec.starts_with('+') || spec.starts_with('-')) {
    if (spec.starts_with('-')) {
      // If spec says minus, then we reverse the multiplication_factor otherwise
      // we use it as is.
      multiplier = (default_sign_for_offset == TZOffset::NEGATIVE) ? 1 : -1;
    }
    spec.remove_prefix(1);
  }

  int hours = 0;
  int minutes = 0;
  int seconds = 0;

  // Parse hours - hh
  cpp::optional<int> result = ParseInt(min_hour, max_hour);
  if (!result)
    return cpp::nullopt;
  hours = result.value();

  // Check for optional minutes.
  // Parse minutes and if there is no data, then default to 0 - hh[:mm]
  if (spec.starts_with(':')) {
    spec.remove_prefix(1);

    // Parse minutes (minimum is 0 and maximum is 59).
    result = ParseInt(0, TimeConstants::MINUTES_PER_HOUR - 1);
    if (!result)
      return cpp::nullopt;
    minutes = result.value();

    // Check for optional seconds.
    // Parse seconds and if there is no data, then default to 0 -  hh[:mm[:ss]]
    if (spec.starts_with(':')) {
      spec.remove_prefix(1);

      // Parse seconds (minimum is 0 and maximum is 59).
      result = ParseInt(0, TimeConstants::SECONDS_PER_MIN - 1);
      if (!result)
        return cpp::nullopt;
      seconds = result.value();
    }
  }
  int32_t offset =
      multiplier * ((((hours * TimeConstants::MINUTES_PER_HOUR) + minutes) *
                     TimeConstants::SECONDS_PER_MIN) +
                    seconds);
  return offset;
}

// (M) the Nth weekday of a month (e.g., the 2nd Sunday in March).
// Mm.w.d
cpp::optional<PosixTransition> PosixTimeZone::ParseMonthWeekWeekday() {
  // Parse month (minimum is 1 and maximum is 12).
  cpp::optional<int> result = ParseInt(1, TimeConstants::MONTHS_PER_YEAR);
  if (!result)
    return cpp::nullopt;
  int month = result.value();

  // Error if there is no dot.
  if (!spec.starts_with('.'))
    return cpp::nullopt;
  spec.remove_prefix(1);

  // Error if there is no week.
  if (spec.empty())
    return cpp::nullopt;

  // Parse week (minimum is 1 and maximum is 5).
  result = ParseInt(1, TimeConstants::MAXIMUM_WEEKS_PER_MONTH);
  if (!result)
    return cpp::nullopt;
  int week = result.value();

  // Error if there is no dot.
  if (!spec.starts_with('.'))
    return cpp::nullopt;
  spec.remove_prefix(1);

  // Error if there is no weekday.
  if (spec.empty())
    return cpp::nullopt;

  // Parse Weekday (minimum is 0 and maximum is 6).
  result = ParseInt(0, TimeConstants::DAYS_PER_WEEK - 1);
  if (!result)
    return cpp::nullopt;
  int weekday = result.value();

  PosixTransition posixTransition;
  posixTransition.date.fmt = PosixTransition::DateFormat::M;
  posixTransition.date.m.month = static_cast<int8_t>(month);
  posixTransition.date.m.week = static_cast<int8_t>(week);
  posixTransition.date.m.weekday = static_cast<int8_t>(weekday);
  return posixTransition;
}

// Parse Jn
// (J) the Nth day of the year (1 <= N <= 365), excluding leap days.
cpp::optional<PosixTransition> PosixTimeZone::ParseNonLeapDay() {
  const cpp::optional<int> result =
      ParseInt(1, TimeConstants::DAYS_PER_NON_LEAP_YEAR);
  if (!result)
    return cpp::nullopt;
  PosixTransition posixTransition;
  posixTransition.date.fmt = PosixTransition::DateFormat::J;
  posixTransition.date.j.day = static_cast<int16_t>(result.value());
  return posixTransition;
}

// Parse n
// (N) the Nth day of the year (0 <= N <= 365), including leap days
cpp::optional<PosixTransition> PosixTimeZone::ParseLeapDay() {
  const cpp::optional<int> result =
      ParseInt(0, TimeConstants::DAYS_PER_LEAP_YEAR - 1);
  if (!result)
    return cpp::nullopt;
  PosixTransition posixTransition;
  posixTransition.date.fmt = PosixTransition::DateFormat::N;
  posixTransition.date.n.day = static_cast<int16_t>(result.value());
  return posixTransition;
}

// datetime = ( Jn | n | Mm.w.d ) [ / offset ]
// The current POSIX spec for America/Los_Angeles is "PST8PDT,M3.2.0,M11.1.0",
// which would be broken down for M11.1.0 as
//     PosixTransition {
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
cpp::optional<PosixTransition> PosixTimeZone::ParseDateTime() {
  PosixTransition posixTransition;
  if (spec.starts_with(',')) {
    spec.remove_prefix(1);

    cpp::optional<PosixTransition> optionalPosixTransition;
    if (spec.starts_with('M')) {
      spec.remove_prefix(1);
      optionalPosixTransition = ParseMonthWeekWeekday(); // Mm.w.d
    } else if (spec.starts_with('J')) {
      spec.remove_prefix(1);
      optionalPosixTransition = ParseNonLeapDay(); // Jn
    } else {
      optionalPosixTransition = ParseLeapDay(); // n
    }
    if (optionalPosixTransition)
      posixTransition = optionalPosixTransition.value();
    else
      return cpp::nullopt;
  }

  // Parse time offset: / offset
  posixTransition.time.offset =
      2 * TimeConstants::SECONDS_PER_HOUR; // default offset is 02:00:00
  if (spec.starts_with('/')) {
    spec.remove_prefix(1);
    // offset value after "/" is always positive.
    TZOffset default_sign_for_offset = TZOffset::POSITIVE;
    const cpp::optional<int32_t> offset =
        ParseOffset(-MAX_HOURS_IN_TRANSITION_TIMES,
                    MAX_HOURS_IN_TRANSITION_TIMES, default_sign_for_offset);
    if (!offset)
      return cpp::nullopt;
    posixTransition.time.offset = offset.value();
  }
  return posixTransition;
}

// The current POSIX spec for America/Los_Angeles is "PST8PDT,M3.2.0,M11.1.0",
// which would be broken down for std_abbr as "PST".
bool PosixTimeZone::UpdateStdAbbr() {
  const auto abbr_result = ParseAbbr();
  if (!abbr_result)
    return false;
  std_abbr = *abbr_result;
  return true;
}

// The current POSIX spec for America/Los_Angeles is "PST8PDT,M3.2.0,M11.1.0",
// which would be broken down for dst_abbr as "PDT".
bool PosixTimeZone::UpdateDstAbbr() {
  const auto abbr_result = ParseAbbr();
  if (!abbr_result)
    return false;
  dst_abbr = *abbr_result;
  return true;
}

// A zone offset is the difference in hours and minutes between a particular
// time zone and UTC. In ISO 8601, the particular zone offset can be indicated
// in a date or time value. The zone offset can be Z for UTC or it can be a
// value "+" or "-" from UTC.
//
// For the current POSIX spec for America/Los_Angeles is
// "PST8PDT,M3.2.0,M11.1.0", which would be broken down for std_offset would be
// -28800. It is same as -1 * 8 * 3600. 8 is the offset.
bool PosixTimeZone::UpdateStdOffset() {
  // offset should be between 0 and 24 hours.
  TZOffset default_sign_for_offset = TZOffset::NEGATIVE;
  // |default_sign_for_offset| for std and dst offset is negative. For example,
  // PST and PDT have default minus sign for the offset (relative to UTC). In,
  // PST8PDT, though 8 doesn't have any sign, but the offset is (-8 * 3600).
  const cpp::optional<int32_t> offset =
      ParseOffset(0, 24, default_sign_for_offset);
  if (!offset)
    return false;
  std_offset = offset.value();
  return true;
}

// For the current POSIX spec for America/Los_Angeles is
// "PST8PDT,M3.2.0,M11.1.0", which would be broken down for dst_offset as
// -25800. It is same as 3600 + (-1 * 8 * 3600).
bool PosixTimeZone::UpdateDstOffset() {
  dst_offset = std_offset + TimeConstants::SECONDS_PER_HOUR; // default
  if (!spec.starts_with(',')) {
    // offset should be between 0 and 24 hours.
    TZOffset default_sign_for_offset = TZOffset::NEGATIVE;
    // |default_sign_for_offset| for std and dst offset is negative. For
    // example, PST and PDT have default minus sign for the offset (relative to
    // UTC). In, PST8PDT, though 8 doesn't have any sign, but the offset is (-8
    // * 3600).
    const cpp::optional<int32_t> offset =
        ParseOffset(0, 24, default_sign_for_offset);
    if (!offset)
      return false;
    dst_offset = offset.value();
  }
  return true;
}

// The current POSIX spec for America/Los_Angeles is "PST8PDT,M3.2.0,M11.1.0",
// which would be broken down as ...
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
bool PosixTimeZone::UpdateDstStart() {
  const auto date_time_result = ParseDateTime();
  if (!date_time_result)
    return false;
  dst_start = *date_time_result;
  return true;
}

// The current POSIX spec for America/Los_Angeles is "PST8PDT,M3.2.0,M11.1.0",
// which would be broken down as ...
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
bool PosixTimeZone::UpdateDstEnd() {
  const auto date_time_result = ParseDateTime();
  if (!date_time_result)
    return false;
  dst_end = *date_time_result;
  return true;
}

bool PosixTimeZone::SpecHasData() {
  if (spec.empty())
    return false;
  return true;
}

// spec = std offset [ dst [ offset ] , datetime , datetime ]
cpp::optional<PosixTimeZone>
PosixTimeZone::ParsePosixSpec(const cpp::string_view spec) {
  if (spec.starts_with(':'))
    return cpp::nullopt;

  PosixTimeZone res(spec);
  if (!res.UpdateStdAbbr())
    return cpp::nullopt;
  if (!res.UpdateStdOffset())
    return cpp::nullopt;
  if (res.spec.empty())
    return res;
  if (!res.UpdateDstAbbr())
    return cpp::nullopt;
  if (!res.UpdateDstOffset())
    return cpp::nullopt;
  if (!res.UpdateDstStart())
    return cpp::nullopt;
  if (!res.UpdateDstEnd())
    return cpp::nullopt;
  if (res.spec.size() != 0)
    return cpp::nullopt;
  return res;
}

} // namespace time_zone_posix
} // namespace LLVM_NAMESPACE
