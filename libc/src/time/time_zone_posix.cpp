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

// TODO(rtenneti): Make ParseInt be a template and return int8_t,
// uint16_t, etc? That way the static_casts below wouldn't be needed.
//
// Parses int value from the string. It returns nullopt if the value
// is less than min or greater than max.
// Note: This is a static method that modifies the input string_view.
cpp::optional<int> PosixTimeZone::Parser::parse_int(cpp::string_view &str,
                                                    int min, int max) {
  if (str.empty())
    return cpp::nullopt;

  char *pEnd;
  long long_value =
      strtol(str.data(), &pEnd, 10); // NOLINT(runtime/deprecated_fn)

  // Check for parsing errors
  if (pEnd == str.data())
    return cpp::nullopt;

  // Explicit cast to int with range check to avoid truncation warning
  if (long_value < static_cast<long>(min) ||
      long_value > static_cast<long>(max))
    return cpp::nullopt;

  int value = static_cast<int>(long_value);

  // Delete the number from the input string.
  size_t len = static_cast<size_t>(pEnd - str.data());
  str.remove_prefix(len);
  return value;
}

// abbr = <.*?> | [^-+,\d]{3,}
cpp::optional<cpp::string_view>
PosixTimeZone::Parser::parse_abbr(cpp::string_view &str) {
  if (str.empty())
    return cpp::nullopt;

  // Handle special zoneinfo <...> form.
  if (str.starts_with('<')) {
    str.remove_prefix(1);
    const auto pos = str.find_first_of('>');
    if (pos == cpp::string_view::npos)
      return cpp::nullopt;
    // Empty quoted names are not allowed
    if (pos == 0)
      return cpp::nullopt;
    cpp::string_view result = str.substr(0, pos);
    // Delete the data up to and including '>' character.
    str.remove_prefix(pos + 1);
    return result;
  }

  size_t len = 0;
  // Handle [^-+,\d<>]{3,}
  // Unquoted abbreviations must not contain: -, +, comma, digits, < or >
  for (const auto &p : str) {
    if (p == '-' || p == '+' || p == ',' || p == '<' || p == '>')
      break;
    if (isdigit(p))
      break;
    len++;
  }
  if (len < 3)
    return cpp::nullopt;
  cpp::string_view result = str.substr(0, len);
  str.remove_prefix(len);
  return result;
}

// offset = [+|-]hh[:mm[:ss]] (aggregated into single seconds value)
// default_sign_for_offset for std and dst offset is negative. For example, PST
// and PDT have default minus sign for the offset (relative to UTC). In,
// PST8PDT, though 8 doesn't have any sign, but the offset is (-8 * 3600).
cpp::optional<int32_t>
PosixTimeZone::Parser::parse_offset(cpp::string_view &str, int min_hour,
                                    int max_hour,
                                    TZOffset default_sign_for_offset) {
  if (str.empty())
    return cpp::nullopt;

  // Handle [+|-].
  int multiplier = default_sign_for_offset;
  if (str.starts_with('+') || str.starts_with('-')) {
    if (str.starts_with('-')) {
      // If spec says minus, then we reverse the multiplication_factor otherwise
      // we use it as is.
      multiplier = (default_sign_for_offset == TZOffset::NEGATIVE) ? 1 : -1;
    }
    str.remove_prefix(1);
  }

  int hours = 0;
  int minutes = 0;
  int seconds = 0;

  // Parse hours - hh
  cpp::optional<int> result = Parser::parse_int(str, min_hour, max_hour);
  if (!result)
    return cpp::nullopt;
  hours = result.value();

  // Check for optional minutes.
  // Parse minutes and if there is no data, then default to 0 - hh[:mm]
  if (str.starts_with(':')) {
    str.remove_prefix(1);

    // Parse minutes (minimum is 0 and maximum is 59).
    result = Parser::parse_int(str, 0, TimeConstants::MINUTES_PER_HOUR - 1);
    if (!result)
      return cpp::nullopt;
    minutes = result.value();

    // Check for optional seconds.
    // Parse seconds and if there is no data, then default to 0 -  hh[:mm[:ss]]
    if (str.starts_with(':')) {
      str.remove_prefix(1);

      // Parse seconds (minimum is 0 and maximum is 59).
      result = Parser::parse_int(str, 0, TimeConstants::SECONDS_PER_MIN - 1);
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
cpp::optional<PosixTransition>
PosixTimeZone::Parser::parse_month_week_weekday(cpp::string_view &str) {
  // Parse month (minimum is 1 and maximum is 12).
  cpp::optional<int> result = parse_int(str, 1, TimeConstants::MONTHS_PER_YEAR);
  if (!result)
    return cpp::nullopt;
  int month = result.value();

  // Error if there is no dot.
  if (!str.starts_with('.'))
    return cpp::nullopt;
  str.remove_prefix(1);

  // Error if there is no week.
  if (str.empty())
    return cpp::nullopt;

  // Parse week (minimum is 1 and maximum is 5).
  result = parse_int(str, 1, TimeConstants::MAXIMUM_WEEKS_PER_MONTH);
  if (!result)
    return cpp::nullopt;
  int week = result.value();

  // Error if there is no dot.
  if (!str.starts_with('.'))
    return cpp::nullopt;
  str.remove_prefix(1);

  // Error if there is no weekday.
  if (str.empty())
    return cpp::nullopt;

  // Parse Weekday (minimum is 0 and maximum is 6).
  result = parse_int(str, 0, TimeConstants::DAYS_PER_WEEK - 1);
  if (!result)
    return cpp::nullopt;
  int weekday = result.value();

  PosixTransition posixTransition;
  posixTransition.date.fmt = PosixTransition::DateFormat::M;
  posixTransition.date.data = PosixTransition::Date::MonthWeekWeekday{
      static_cast<int8_t>(month), static_cast<int8_t>(week),
      static_cast<int8_t>(weekday)};
  return posixTransition;
}

// Parse Jn
// (J) the Nth day of the year (1 <= N <= 365), excluding leap days.
cpp::optional<PosixTransition>
PosixTimeZone::Parser::parse_non_leap_day(cpp::string_view &str) {
  const cpp::optional<int> result =
      parse_int(str, 1, TimeConstants::DAYS_PER_NON_LEAP_YEAR);
  if (!result)
    return cpp::nullopt;
  PosixTransition posixTransition;
  posixTransition.date.fmt = PosixTransition::DateFormat::J;
  posixTransition.date.data =
      PosixTransition::Date::NonLeapDay{static_cast<int16_t>(result.value())};
  return posixTransition;
}

// Parse n
// (N) the Nth day of the year (0 <= N <= 365), including leap days
cpp::optional<PosixTransition>
PosixTimeZone::Parser::parse_leap_day(cpp::string_view &str) {
  const cpp::optional<int> result =
      parse_int(str, 0, TimeConstants::DAYS_PER_LEAP_YEAR - 1);
  if (!result)
    return cpp::nullopt;
  PosixTransition posixTransition;
  posixTransition.date.fmt = PosixTransition::DateFormat::N;
  posixTransition.date.data =
      PosixTransition::Date::Day{static_cast<int16_t>(result.value())};
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
cpp::optional<PosixTransition>
PosixTimeZone::Parser::parse_date_time(cpp::string_view &str) {
  PosixTransition posixTransition;
  if (str.starts_with(',')) {
    str.remove_prefix(1);

    if (str.starts_with('M')) {
      str.remove_prefix(1);
      auto result = parse_month_week_weekday(str); // Mm.w.d
      if (result) {
        posixTransition = result.value();
      } else {
        return cpp::nullopt;
      }
    } else if (str.starts_with('J')) {
      str.remove_prefix(1);
      auto result = parse_non_leap_day(str); // Jn
      if (result) {
        posixTransition = result.value();
      } else {
        return cpp::nullopt;
      }
    } else {
      auto result = parse_leap_day(str); // n
      if (result) {
        posixTransition = result.value();
      } else {
        return cpp::nullopt;
      }
    }
  }

  // Parse time offset: / offset
  posixTransition.time.offset =
      2 * TimeConstants::SECONDS_PER_HOUR; // default offset is 02:00:00
  if (str.starts_with('/')) {
    str.remove_prefix(1);
    // offset value after "/" is always positive.
    TZOffset default_sign_for_offset = TZOffset::POSITIVE;
    const cpp::optional<int32_t> offset =
        parse_offset(str, -MAX_HOURS_IN_TRANSITION_TIMES,
                     MAX_HOURS_IN_TRANSITION_TIMES, default_sign_for_offset);
    if (!offset)
      return cpp::nullopt;
    posixTransition.time.offset = offset.value();
  }
  return posixTransition;
}

// spec = std offset [ dst [ offset ] , datetime , datetime ]
cpp::optional<PosixTimeZone>
PosixTimeZone::parse_posix_spec(const cpp::string_view spec_input) {
  // Reject colon-prefix (implementation-defined per POSIX).
  // Most Unix systems use ":America/New_York" to load IANA timezone database
  // files (tzfile format). This implementation currently only supports POSIX
  // TZ rules, not tzfile loading. Future tzfile support is planned (Phase 7).
  if (spec_input.starts_with(':'))
    return cpp::nullopt;

  // Create a mutable copy of the spec for parsing
  cpp::string_view spec = spec_input;

  // Create result object with original spec preserved
  PosixTimeZone res;
  res.original_spec = spec_input;
  res.spec = ""; // Will remain empty to indicate parsing is complete

  // Parse standard timezone abbreviation (e.g., "PST" from
  // "PST8PDT,M3.2.0,M11.1.0") This extracts the name of the standard
  // (non-daylight-saving) timezone.
  const auto std_abbr_result = Parser::parse_abbr(spec);
  if (!std_abbr_result)
    return cpp::nullopt;
  res.std_abbr = *std_abbr_result;

  // Parse standard timezone offset (e.g., "8" from "PST8PDT,M3.2.0,M11.1.0")
  // This is the offset from UTC in hours (can include minutes and seconds).
  // For PST8, the offset is -28800 seconds (8 hours west of UTC).
  TZOffset default_sign_for_offset = TZOffset::NEGATIVE;
  const cpp::optional<int32_t> std_offset =
      Parser::parse_offset(spec, 0, 24, default_sign_for_offset);
  if (!std_offset)
    return cpp::nullopt;
  res.std_offset = std_offset.value();

  // If no DST info, return early (e.g., "EST5" has no DST component)
  if (spec.empty())
    return res;

  // Parse DST timezone abbreviation (e.g., "PDT" from "PST8PDT,M3.2.0,M11.1.0")
  // This extracts the name of the daylight-saving timezone.
  const auto dst_abbr_result = Parser::parse_abbr(spec);
  if (!dst_abbr_result)
    return cpp::nullopt;
  res.dst_abbr = *dst_abbr_result;

  // Parse DST timezone offset (optional, defaults to std_offset + 1 hour)
  // If omitted, DST is assumed to be 1 hour ahead of standard time.
  // For "PST8PDT", PDT defaults to -25200 seconds (7 hours west of UTC).
  res.dst_offset = res.std_offset + TimeConstants::SECONDS_PER_HOUR;
  if (!spec.starts_with(',')) {
    const cpp::optional<int32_t> dst_offset =
        Parser::parse_offset(spec, 0, 24, default_sign_for_offset);
    if (!dst_offset)
      return cpp::nullopt;
    res.dst_offset = dst_offset.value();
  }

  // Parse DST start date/time (e.g., "M3.2.0" from "PST8PDT,M3.2.0,M11.1.0")
  // M3.2.0 means: 2nd Sunday (week 2, weekday 0) of March (month 3)
  // Default time is 02:00:00 if no time offset is specified.
  const auto dst_start_result = Parser::parse_date_time(spec);
  if (!dst_start_result)
    return cpp::nullopt;
  res.dst_start = *dst_start_result;

  // Parse DST end date/time (e.g., "M11.1.0" from "PST8PDT,M3.2.0,M11.1.0")
  // M11.1.0 means: 1st Sunday (week 1, weekday 0) of November (month 11)
  // Default time is 02:00:00 if no time offset is specified.
  const auto dst_end_result = Parser::parse_date_time(spec);
  if (!dst_end_result)
    return cpp::nullopt;
  res.dst_end = *dst_end_result;

  // Ensure all spec was consumed
  if (spec.size() != 0)
    return cpp::nullopt;

  return res;
}

// Helper function to calculate the day of year for a transition date
static int calculate_transition_day(const PosixTransition &transition,
                                    int year) {
  using LIBC_NAMESPACE::time_utils::is_leap_year;

  const auto &date = transition.date;

  if (date.fmt == PosixTransition::DateFormat::J) {
    // Julian day (1-365), excluding leap days
    const auto &non_leap =
        cpp::get<PosixTransition::Date::NonLeapDay>(date.data);
    return non_leap.day - 1; // Convert to 0-indexed
  }

  if (date.fmt == PosixTransition::DateFormat::N) {
    // Zero-based day of year (0-365), including leap days
    const auto &day = cpp::get<PosixTransition::Date::Day>(date.data);
    return day.day;
  }

  // DateFormat::M - Mm.w.d format (month/week/weekday)
  const auto &mwd =
      cpp::get<PosixTransition::Date::MonthWeekWeekday>(date.data);

  // Days in each month (non-leap year)
  static constexpr int days_in_month[12] = {31, 28, 31, 30, 31, 30,
                                            31, 31, 30, 31, 30, 31};

  // Calculate day of year for the 1st day of the target month
  int day_of_year = 0;
  for (int m = 0; m < mwd.month - 1; ++m) {
    day_of_year += days_in_month[m];
    // Add leap day if we're past February in a leap year
    if (m == 1 && is_leap_year(year))
      day_of_year++;
  }

  // Find the first occurrence of the target weekday in the month
  // We need to know what day of the week the 1st of the month is
  // For simplicity, we'll calculate it using the epoch

  // Calculate days since epoch for Jan 1 of this year
  int days_since_epoch = 0;
  for (int y = 1970; y < year; ++y) {
    days_since_epoch += is_leap_year(y) ? 366 : 365;
  }
  days_since_epoch += day_of_year;

  // Jan 1, 1970 was a Thursday (day 4)
  int first_weekday = (4 + days_since_epoch) % 7;

  // Find the first occurrence of target weekday
  int days_until_target = (mwd.weekday - first_weekday + 7) % 7;
  int first_occurrence = days_until_target;

  // Add weeks to get to the desired week
  // Week 1 = first occurrence, Week 2 = second occurrence, etc.
  // Week 5 = last occurrence (might be week 4 if not present)
  int target_day = first_occurrence + (mwd.week - 1) * 7;

  // Get the number of days in this month
  int month_days = days_in_month[mwd.month - 1];
  if (mwd.month == 2 && is_leap_year(year))
    month_days = 29;

  // If week 5 and we've gone past the end of month, use last occurrence
  if (mwd.week == 5 && target_day >= month_days) {
    target_day -= 7;
  }

  return day_of_year + target_day;
}

// Helper function to calculate time_t for a specific transition
static time_t calculate_transition_time(const PosixTransition &transition,
                                        int year) {
  using LIBC_NAMESPACE::time_utils::is_leap_year;

  int day_of_year = calculate_transition_day(transition, year);

  // Calculate days since epoch
  int days_since_epoch = 0;
  for (int y = 1970; y < year; ++y) {
    days_since_epoch += is_leap_year(y) ? 366 : 365;
  }
  days_since_epoch += day_of_year;

  // Convert to seconds and add the transition time offset
  return static_cast<time_t>(days_since_epoch) *
             TimeConstants::SECONDS_PER_DAY +
         transition.time.offset;
}

bool PosixTimeZone::is_dst_active(time_t time) const {
  // If no DST rules, DST is never active
  if (dst_abbr.empty())
    return false;

  // Handle edge cases: very old or very far future times
  // DST rules didn't exist before 1900 and won't be meaningful after 3000
  if (time < -2208988800LL || time > 32503680000LL) {
    // Before 1900 or after 3000 - assume standard time
    return false;
  }

  // Convert time_t to year to calculate transition times for that year
  // This is a simplified calculation - we just need the year
  int64_t days_since_epoch = time / TimeConstants::SECONDS_PER_DAY;

  // Handle negative time_t (before 1970)
  int year = 1970;
  if (days_since_epoch < 0) {
    // Count backwards from 1970
    while (days_since_epoch < 0) {
      year--;
      int days_in_year =
          LIBC_NAMESPACE::time_utils::is_leap_year(year) ? 366 : 365;
      days_since_epoch += days_in_year;
    }
  } else {
    // Count forward from 1970
    while (true) {
      int days_in_year =
          LIBC_NAMESPACE::time_utils::is_leap_year(year) ? 366 : 365;
      if (days_since_epoch < days_in_year)
        break;
      days_since_epoch -= days_in_year;
      year++;
      // Safety check to prevent infinite loops
      if (year > 3000)
        return false;
    }
  }

  // Calculate the transition times for this year
  time_t dst_start_time = calculate_transition_time(dst_start, year);
  time_t dst_end_time = calculate_transition_time(dst_end, year);

  // Check if DST is active
  // Note: In southern hemisphere, dst_start > dst_end (DST starts in fall, ends
  // in spring)
  if (dst_start_time < dst_end_time) {
    // Northern hemisphere: DST is active between start and end
    return time >= dst_start_time && time < dst_end_time;
  } else {
    // Southern hemisphere: DST is active outside the end-to-start period
    return time >= dst_start_time || time < dst_end_time;
  }
}

int32_t PosixTimeZone::get_timezone_adjustment(cpp::string_view tz_spec,
                                             time_t time) {
  // If TZ spec is empty, return 0 (use UTC)
  if (tz_spec.empty())
    return 0;

  // Parse the TZ specification
  auto parsed = parse_posix_spec(tz_spec);
  if (!parsed)
    return 0; // Invalid TZ spec, fall back to UTC

  const auto &tz = *parsed;

  // Determine if DST is active and return appropriate offset
  if (tz.is_dst_active(time))
    return tz.dst_offset;
  else
    return tz.std_offset;
}

} // namespace time_zone_posix
} // namespace LIBC_NAMESPACE
