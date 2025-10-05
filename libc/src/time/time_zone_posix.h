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
#include "src/__support/CPP/variant.h"

namespace LIBC_NAMESPACE {
namespace time_zone_posix {

// This enum is used to handle + or - symbol in the offset specification of
// TZ, which is of the format "[+|-]hh[:mm[:ss]]".
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
// POSIX TZ Specification Format:
//   std offset [dst [offset] [,start[/time],end[/time]]]
//
// Where:
//   - std: Standard timezone abbreviation (3+ chars, or <...> for special
//   chars)
//   - offset: Hours[[:minutes][:seconds]] west of UTC
//   - dst: Optional DST timezone abbreviation
//   - start/end: Transition dates (Jn, n, or Mm.w.d format)
//   - time: Transition time (default 02:00:00)
//
// Colon-Prefix Behavior:
//   If TZ starts with ':', POSIX specifies this as "implementation-defined".
//   Most Unix systems treat ":America/New_York" as a path to load IANA timezone
//   database files (tzfile format) from /usr/share/zoneinfo/.
//
//   THIS IMPLEMENTATION currently rejects colon-prefixed strings and returns
//   nullopt. This parser only handles POSIX TZ rules, not tzfile loading.
//   Future support for standard Unix tzfile loading is planned (see Phase 7).
//
//   Users should specify POSIX TZ rules directly, for example:
//     - Instead of ":America/New_York", use "EST5EDT,M3.2.0,M11.1.0"
//     - Instead of ":UTC", use "UTC0"
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
/// Represents a DST transition date and time in POSIX TZ format.
///
/// The date/time of the transition. The date is specified as either:
/// - (J) the Nth day of the year (1 <= N <= 365), excluding leap days, or
/// - (N) the Nth day of the year (0 <= N <= 365), including leap days, or
/// - (M) the Nth weekday of a month (e.g., the 2nd Sunday in March).
///
/// The time, specified as a day offset, identifies the particular moment
/// of the transition, and may be negative or >= 24h, in which case
/// it would take us to another day, and perhaps week, or even month.
class PosixTransition {
public:
  /// Format specifier for the transition date.
  enum class DateFormat : int { J, N, M };

  /// Represents the date component of a transition.
  struct Date {
    /// Julian day (1-365), excluding leap days (J format).
    struct NonLeapDay {
      int16_t day; // day of non-leap year [1:365]
    };
    /// Zero-based day of year (0-365), including leap days (N format).
    struct Day {
      int16_t day; // day of year [0:365]
    };
    /// Nth occurrence of weekday in a month (M format).
    struct MonthWeekWeekday {
      int8_t month;   // month of year [1:12]
      int8_t week;    // week of month [1:5] (5==last)
      int8_t weekday; // 0==Sun, ..., 6=Sat
    };

    DateFormat fmt;
    cpp::variant<NonLeapDay, Day, MonthWeekWeekday> data;
  };

  /// Represents the time component of a transition.
  struct Time {
    int32_t offset; // seconds before/after 00:00:00
  };

  Date date; ///< The date when the transition occurs.
  Time time; ///< The time when the transition occurs.

  PosixTransition() {
    date.fmt = DateFormat::N;
    date.data = Date::Day{0};
    time.offset = 0;
  }

  explicit PosixTransition(DateFormat fmt, int16_t day, int32_t offset) {
    date.fmt = fmt;
    if (fmt == DateFormat::J) {
      date.data = Date::NonLeapDay{day};
    } else { // DateFormat::N
      date.data = Date::Day{day};
    }
    time.offset = offset;
  }

  explicit PosixTransition(DateFormat fmt, int8_t month, int8_t week,
                           int8_t weekday, int32_t offset) {
    date.fmt = fmt;
    date.data = Date::MonthWeekWeekday{month, week, weekday};
    time.offset = offset;
  }
};

/// Represents a POSIX-format timezone specification.
///
/// The entirety of a POSIX-string specified time-zone rule. The standard
/// abbreviation and offset are always given. If the time zone includes
/// daylight saving, then the daylight abbreviation is non-empty and the
/// remaining fields are also valid. Note that the start/end transitions
/// are not ordered---in the southern hemisphere the transition to end
/// daylight time occurs first in any particular year.
class PosixTimeZone {
public:
  /// Default constructor for testing.
  PosixTimeZone()
      : spec(""), original_spec(""), std_abbr("UTC"), std_offset(0),
        dst_abbr(""), dst_offset(0) {}

  explicit PosixTimeZone(const cpp::string_view &spec)
      : spec(spec), original_spec(spec), std_abbr("UTC"), std_offset(0),
        dst_abbr(""), dst_offset(0) {}

  explicit PosixTimeZone(const cpp::string_view &spec,
                         cpp::string_view std_abbr, int32_t std_offset,
                         cpp::string_view dst_abbr, int32_t dst_offset,
                         time_zone_posix::PosixTransition dst_start,
                         time_zone_posix::PosixTransition dst_end)
      : spec(spec), original_spec(spec), std_abbr(std_abbr),
        std_offset(std_offset), dst_abbr(dst_abbr), dst_offset(dst_offset),
        dst_start(dst_start), dst_end(dst_end) {}

  /// Parses a POSIX time-zone specification into its constituent pieces.
  ///
  /// Parses the TZ environment variable format defined by POSIX, filling in
  /// any missing values (DST offset, or start/end transition times) with the
  /// standard-defined defaults.
  ///
  /// \param spec The POSIX TZ specification string to parse.
  /// \return cpp::optional<PosixTimeZone> containing the parsed timezone on
  ///         success, or cpp::nullopt if parsing fails.
  ///
  /// \note Colon-prefixed strings (e.g., ":America/New_York") are rejected.
  ///       These are implementation-defined per POSIX and typically used to
  ///       load IANA timezone database files. This implementation currently
  ///       only supports POSIX TZ rules. Use "EST5EDT,M3.2.0,M11.1.0" instead.
  static cpp::optional<PosixTimeZone>
  ParsePosixSpec(const cpp::string_view spec);

  cpp::string_view spec; ///< Mutable parse position (modified during parsing)
  cpp::string_view
      original_spec; ///< Immutable original for string_view stability

  cpp::string_view std_abbr; ///< Standard time abbreviation (e.g., "PST")
  int32_t std_offset;        ///< Standard time offset from UTC in seconds

  cpp::string_view dst_abbr; ///< DST abbreviation (e.g., "PDT"), empty if no DST
  int32_t dst_offset;        ///< DST offset from UTC in seconds

  PosixTransition dst_start; ///< When DST begins
  PosixTransition dst_end;   ///< When DST ends

  /// Helper class for non-destructive parsing.
  ///
  /// Maintains parse position without modifying the original spec string.
  class Parser {
  private:
    cpp::string_view remaining; // Current position in parse (mutable)
    const cpp::string_view original; // Full original spec (immutable)
    size_t position; // Current offset into original string
    
  public:
    explicit Parser(cpp::string_view spec)
        : remaining(spec), original(spec), position(0) {}
    
    /// Advance the parse position by n characters.
    void advance(size_t n) {
      remaining.remove_prefix(n);
      position += n;
    }
    
    /// Check if there's more data to parse.
    bool has_more() const { return !remaining.empty(); }
    
    /// Get the current parse position (offset into original string).
    size_t current_position() const { return position; }
    
    /// Get the remaining unparsed string.
    cpp::string_view get_remaining() const { return remaining; }
    
    /// Get the original full spec string.
    cpp::string_view get_original() const { return original; }

    /// Parse integer value from string.
    /// \param str String to parse from (updated on success).
    /// \param min Minimum valid value.
    /// \param max Maximum valid value.
    /// \return Parsed integer or nullopt if out of range or invalid.
    static cpp::optional<int> parse_int(cpp::string_view &str, int min,
                                        int max);
    
    /// Parse timezone abbreviation from string.
    /// \param str String to parse from (updated on success).
    /// \return Parsed abbreviation or nullopt if invalid.
    static cpp::optional<cpp::string_view> parse_abbr(cpp::string_view &str);
    
    /// Parse timezone offset in format [+|-]hh[:mm[:ss]].
    /// \param str String to parse from (updated on success).
    /// \param min_hour Minimum valid hour value.
    /// \param max_hour Maximum valid hour value.
    /// \param default_sign_for_offset Default sign if not specified.
    /// \return Offset in seconds or nullopt if invalid.
    static cpp::optional<int32_t> parse_offset(cpp::string_view &str,
                                                int min_hour, int max_hour,
                                                TZOffset default_sign_for_offset);

    /// Parse Mm.w.d format (Nth occurrence of weekday in month).
    /// \param str String to parse from (updated on success).
    /// \return Parsed transition or nullopt if invalid.
    static cpp::optional<PosixTransition>
    parse_month_week_weekday(cpp::string_view &str);

    /// Parse Jn format (Julian day, excluding leap days).
    /// \param str String to parse from (updated on success).
    /// \return Parsed transition or nullopt if invalid.
    static cpp::optional<PosixTransition>
    parse_non_leap_day(cpp::string_view &str);

    /// Parse n format (day of year, including leap days).
    /// \param str String to parse from (updated on success).
    /// \return Parsed transition or nullopt if invalid.
    static cpp::optional<PosixTransition> parse_leap_day(cpp::string_view &str);

    /// Parse date/time transition in format: (Jn | n | Mm.w.d)[/offset].
    /// \param str String to parse from (updated on success).
    /// \return Parsed transition or nullopt if invalid.
    static cpp::optional<PosixTransition>
    parse_date_time(cpp::string_view &str);
  };
};

} // namespace time_zone_posix
} // namespace LIBC_NAMESPACE

#endif // LLVM_LIBC_SRC_TIME_TIME_ZONE_POSIX_H
