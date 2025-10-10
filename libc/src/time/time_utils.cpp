//===-- Implementation of mktime function ---------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/time/time_utils.h"
#include "hdr/stdint_proxy.h"
#include "src/__support/CPP/limits.h" // INT_MIN, INT_MAX
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/time/time_constants.h"
#include "src/time/time_zone_posix.h"
#include "src/time/tz_env_access.h"

namespace LIBC_NAMESPACE_DECL {
namespace time_utils {

// TODO: clean this up in a followup patch
cpp::optional<time_t> mktime_internal(const tm *tm_out) {
  // Unlike most C Library functions, mktime doesn't just die on bad input.
  // TODO(rtenneti); Handle leap seconds.
  int64_t tm_year_from_base = tm_out->tm_year + time_constants::TIME_YEAR_BASE;

  // 32-bit end-of-the-world is 03:14:07 UTC on 19 January 2038.
  if (sizeof(time_t) == 4 &&
      tm_year_from_base >= time_constants::END_OF32_BIT_EPOCH_YEAR) {
    if (tm_year_from_base > time_constants::END_OF32_BIT_EPOCH_YEAR)
      return cpp::nullopt;
    if (tm_out->tm_mon > 0)
      return cpp::nullopt;
    if (tm_out->tm_mday > 19)
      return cpp::nullopt;
    else if (tm_out->tm_mday == 19) {
      if (tm_out->tm_hour > 3)
        return cpp::nullopt;
      else if (tm_out->tm_hour == 3) {
        if (tm_out->tm_min > 14)
          return cpp::nullopt;
        else if (tm_out->tm_min == 14) {
          if (tm_out->tm_sec > 7)
            return cpp::nullopt;
        }
      }
    }
  }

  // Years are ints.  A 32-bit year will fit into a 64-bit time_t.
  // A 64-bit year will not.
  static_assert(
      sizeof(int) == 4,
      "ILP64 is unimplemented. This implementation requires 32-bit integers.");

  // Calculate number of months and years from tm_mon.
  int64_t month = tm_out->tm_mon;
  if (month < 0 || month >= time_constants::MONTHS_PER_YEAR - 1) {
    int64_t years = month / 12;
    month %= 12;
    if (month < 0) {
      years--;
      month += 12;
    }
    tm_year_from_base += years;
  }
  bool tm_year_is_leap = time_utils::is_leap_year(tm_year_from_base);

  // Calculate total number of days based on the month and the day (tm_mday).
  int64_t total_days = tm_out->tm_mday - 1;
  for (int64_t i = 0; i < month; ++i)
    total_days += time_constants::NON_LEAP_YEAR_DAYS_IN_MONTH[i];
  // Add one day if it is a leap year and the month is after February.
  if (tm_year_is_leap && month > 1)
    total_days++;

  // Calculate total numbers of days based on the year.
  total_days += (tm_year_from_base - time_constants::EPOCH_YEAR) *
                time_constants::DAYS_PER_NON_LEAP_YEAR;
  if (tm_year_from_base >= time_constants::EPOCH_YEAR) {
    total_days +=
        time_utils::get_num_of_leap_years_before(tm_year_from_base - 1) -
        time_utils::get_num_of_leap_years_before(time_constants::EPOCH_YEAR);
  } else if (tm_year_from_base >= 1) {
    total_days -=
        time_utils::get_num_of_leap_years_before(time_constants::EPOCH_YEAR) -
        time_utils::get_num_of_leap_years_before(tm_year_from_base - 1);
  } else {
    // Calculate number of leap years until 0th year.
    total_days -=
        time_utils::get_num_of_leap_years_before(time_constants::EPOCH_YEAR) -
        time_utils::get_num_of_leap_years_before(0);
    if (tm_year_from_base <= 0) {
      total_days -= 1; // Subtract 1 for 0th year.
      // Calculate number of leap years until -1 year
      if (tm_year_from_base < 0) {
        total_days -=
            time_utils::get_num_of_leap_years_before(-tm_year_from_base) -
            time_utils::get_num_of_leap_years_before(1);
      }
    }
  }

  // Calculate time_t treating the input struct tm as if it were in local time
  time_t local_seconds = static_cast<time_t>(
      tm_out->tm_sec + tm_out->tm_min * time_constants::SECONDS_PER_MIN +
      tm_out->tm_hour * time_constants::SECONDS_PER_HOUR +
      total_days * time_constants::SECONDS_PER_DAY);

  // Get TZ environment variable using abstraction layer
  const char *tz_env = time_internal::get_tz_env();
  cpp::string_view tz_spec = tz_env ? cpp::string_view(tz_env) : "";

  // Convert from local time to UTC by subtracting the timezone adjustment.
  //
  // Challenge: get_timezone_adjustment() needs a time_t parameter to determine
  // if DST is active, but we're trying to *calculate* that time_t. This is a
  // chicken-and-egg problem that requires iteration to solve.
  //
  // Solution: Two-pass algorithm
  // 1. First pass: Treat local_seconds as if it were UTC to get an initial
  //    adjustment. This gives us a rough approximation of the actual UTC time.
  // 2. Second pass: Use the approximation from step 1 to get the correct
  //    adjustment for the actual UTC time, then recalculate.
  //
  // This converges because the DST boundary won't shift more than a few hours
  // between iterations, so the second iteration gives us the correct answer.

  // First approximation: pretend local_seconds is UTC to get initial offset
  int32_t adjustment = time_zone_posix::PosixTimeZone::get_timezone_adjustment(
      tz_spec, local_seconds);
  time_t utc_seconds = local_seconds - adjustment;

  // Second iteration: use our approximation to get the correct offset
  // This handles cases where the first approximation crossed a DST boundary
  adjustment = time_zone_posix::PosixTimeZone::get_timezone_adjustment(
      tz_spec, utc_seconds);
  utc_seconds = local_seconds - adjustment;

  // Update tm_isdst to indicate whether DST is active for this time
  // We determine this by checking if DST rules exist and calling is_dst_active
  if (!tz_spec.empty()) {
    auto tz_parsed = time_zone_posix::PosixTimeZone::parse_posix_spec(tz_spec);
    if (tz_parsed.has_value()) {
      // Check if DST is active for the final UTC time
      bool is_dst = tz_parsed->is_dst_active(utc_seconds);
      // Need to cast away const to update tm_isdst
      // This is allowed per POSIX: mktime() normalizes all tm fields
      const_cast<tm *>(tm_out)->tm_isdst = is_dst ? 1 : 0;
    }
  }

  return utc_seconds;
}

static int64_t computeRemainingYears(int64_t daysPerYears,
                                     int64_t quotientYears,
                                     int64_t *remainingDays) {
  int64_t years = *remainingDays / daysPerYears;
  if (years == quotientYears)
    years--;
  *remainingDays -= years * daysPerYears;
  return years;
}

// First, divide "total_seconds" by the number of seconds in a day to get the
// number of days since Jan 1 1970. The remainder will be used to calculate the
// number of Hours, Minutes and Seconds.
//
// Then, adjust that number of days by a constant to be the number of days
// since Mar 1 2000. Year 2000 is a multiple of 400, the leap year cycle. This
// makes it easier to count how many leap years have passed using division.
//
// While calculating numbers of years in the days, the following algorithm
// subdivides the days into the number of 400 years, the number of 100 years and
// the number of 4 years. These numbers of cycle years are used in calculating
// leap day. This is similar to the algorithm used in  getNumOfLeapYearsBefore()
// and isLeapYear(). Then compute the total number of years in days from these
// subdivided units.
//
// Compute the number of months from the remaining days. Finally, adjust years
// to be 1900 and months to be from January.
int64_t update_from_seconds(time_t total_seconds, tm *tm) {
  // Days in month starting from March in the year 2000.
  static const char daysInMonth[] = {31 /* Mar */, 30, 31, 30, 31, 31,
                                     30,           31, 30, 31, 31, 29};

  constexpr time_t time_min =
      (sizeof(time_t) == 4)
          ? INT_MIN
          : INT_MIN * static_cast<int64_t>(
                          time_constants::NUMBER_OF_SECONDS_IN_LEAP_YEAR);
  constexpr time_t time_max =
      (sizeof(time_t) == 4)
          ? INT_MAX
          : INT_MAX * static_cast<int64_t>(
                          time_constants::NUMBER_OF_SECONDS_IN_LEAP_YEAR);

  if (total_seconds < time_min || total_seconds > time_max)
    return time_utils::out_of_range();

  int64_t seconds =
      total_seconds - time_constants::SECONDS_UNTIL2000_MARCH_FIRST;
  int64_t days = seconds / time_constants::SECONDS_PER_DAY;
  int64_t remainingSeconds = seconds % time_constants::SECONDS_PER_DAY;
  if (remainingSeconds < 0) {
    remainingSeconds += time_constants::SECONDS_PER_DAY;
    days--;
  }

  int64_t wday = (time_constants::WEEK_DAY_OF2000_MARCH_FIRST + days) %
                 time_constants::DAYS_PER_WEEK;
  if (wday < 0)
    wday += time_constants::DAYS_PER_WEEK;

  // Compute the number of 400 year cycles.
  int64_t numOfFourHundredYearCycles = days / time_constants::DAYS_PER400_YEARS;
  int64_t remainingDays = days % time_constants::DAYS_PER400_YEARS;
  if (remainingDays < 0) {
    remainingDays += time_constants::DAYS_PER400_YEARS;
    numOfFourHundredYearCycles--;
  }

  // The remaining number of years after computing the number of
  // "four hundred year cycles" will be 4 hundred year cycles or less in 400
  // years.
  int64_t numOfHundredYearCycles = computeRemainingYears(
      time_constants::DAYS_PER100_YEARS, 4, &remainingDays);

  // The remaining number of years after computing the number of
  // "hundred year cycles" will be 25 four year cycles or less in 100 years.
  int64_t numOfFourYearCycles = computeRemainingYears(
      time_constants::DAYS_PER4_YEARS, 25, &remainingDays);

  // The remaining number of years after computing the number of
  // "four year cycles" will be 4 one year cycles or less in 4 years.
  int64_t remainingYears = computeRemainingYears(
      time_constants::DAYS_PER_NON_LEAP_YEAR, 4, &remainingDays);

  // Calculate number of years from year 2000.
  int64_t years = remainingYears + 4 * numOfFourYearCycles +
                  100 * numOfHundredYearCycles +
                  400LL * numOfFourHundredYearCycles;

  int leapDay =
      !remainingYears && (numOfFourYearCycles || !numOfHundredYearCycles);

  // We add 31 and 28 for the number of days in January and February, since our
  // starting point was March 1st.
  int64_t yday = remainingDays + 31 + 28 + leapDay;
  if (yday >= time_constants::DAYS_PER_NON_LEAP_YEAR + leapDay)
    yday -= time_constants::DAYS_PER_NON_LEAP_YEAR + leapDay;

  int64_t months = 0;
  while (daysInMonth[months] <= remainingDays) {
    remainingDays -= daysInMonth[months];
    months++;
  }

  if (months >= time_constants::MONTHS_PER_YEAR - 2) {
    months -= time_constants::MONTHS_PER_YEAR;
    years++;
  }

  if (years > INT_MAX || years < INT_MIN)
    return time_utils::out_of_range();

  // All the data (years, month and remaining days) was calculated from
  // March, 2000. Thus adjust the data to be from January, 1900.
  tm->tm_year = static_cast<int>(years + 2000 - time_constants::TIME_YEAR_BASE);
  tm->tm_mon = static_cast<int>(months + 2);
  tm->tm_mday = static_cast<int>(remainingDays + 1);
  tm->tm_wday = static_cast<int>(wday);
  tm->tm_yday = static_cast<int>(yday);

  tm->tm_hour =
      static_cast<int>(remainingSeconds / time_constants::SECONDS_PER_HOUR);
  tm->tm_min =
      static_cast<int>(remainingSeconds / time_constants::SECONDS_PER_MIN %
                       time_constants::SECONDS_PER_MIN);
  tm->tm_sec =
      static_cast<int>(remainingSeconds % time_constants::SECONDS_PER_MIN);

  // Update tm_isdst based on TZ environment variable using abstraction layer
  const char *tz_env = time_internal::get_tz_env();
  cpp::string_view tz_spec = tz_env ? cpp::string_view(tz_env) : "";

  if (!tz_spec.empty()) {
    auto tz_parsed = time_zone_posix::PosixTimeZone::parse_posix_spec(tz_spec);
    if (tz_parsed.has_value()) {
      bool is_dst = tz_parsed->is_dst_active(total_seconds);
      tm->tm_isdst = is_dst ? 1 : 0;
    } else {
      tm->tm_isdst = 0;
    }
  } else {
    tm->tm_isdst = 0;
  }

  return 0;
}

} // namespace time_utils
} // namespace LIBC_NAMESPACE_DECL
