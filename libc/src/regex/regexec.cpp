//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of regexec.
///
//===----------------------------------------------------------------------===//

#include "src/regex/regexec.h"

#include "hdr/regex_macros.h"
#include "hdr/types/regoff_t.h"
#include "src/__support/CPP/limits.h"
#include "src/__support/CPP/mutex.h"
#include "src/__support/CPP/span.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/common.h"
#include "src/__support/libc_assert.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/null_check.h"
#include "src/__support/regex/regex_internal.h"
#include "src/__support/regex/regex_matcher.h"

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(int, regexec,
                   (const regex_t *__restrict preg,
                    const char *__restrict string, size_t nmatch,
                    regmatch_t *__restrict pmatch, int eflags)) {
  LIBC_CRASH_ON_NULLPTR(preg);
  LIBC_CRASH_ON_NULLPTR(string);

  // TODO: Enable REG_STARTEND input slicing in a follow-up change.
  if ((eflags & ~(REG_NOTBOL | REG_NOTEOL)) != 0)
    return REG_BADPAT;

  // Guard against a null internal pointer (e.g. called after regfree).
  auto *internal = static_cast<regex::RegexInternal *>(preg->__internal);
  if (!internal)
    return REG_NOMATCH;

  const bool want_pmatch = nmatch > 0 && !(internal->cflags & REG_NOSUB);
  if (want_pmatch)
    LIBC_CRASH_ON_NULLPTR(pmatch);
  const cpp::span<regmatch_t> matches =
      want_pmatch ? cpp::span<regmatch_t>(pmatch, nmatch)
                  : cpp::span<regmatch_t>();
  // TODO: Populate subgroup offsets pmatch[1..k] in a follow-up change.  Until
  // then, fail closed with REG_BADPAT on match rather than returning 0 with
  // {-1, -1} in requested subgroup slots.
  const bool unsupported_subgroups =
      want_pmatch && nmatch > 1 && preg->re_nsub > 0;
  const bool need_span = want_pmatch && !unsupported_subgroups;

  // POSIX requires regexec to be thread-safe across concurrent calls sharing a
  // const regex_t*.  Compute string length outside the lock, serialise access
  // to internal->pool during lazy derivative evaluation, then release the lock
  // before writing caller-owned pmatch.
  const cpp::string_view input(string);
  regex::MatchSpan span{};
  regex::MatchResult res = regex::MatchResult::NoMatch;
  {
    cpp::lock_guard guard(internal->lock);
    res = need_span ? regex::match_span(*internal, input, span, eflags)
                    : regex::match(*internal, input, eflags);
  }
  switch (res) {
  case regex::MatchResult::Match:
    if (unsupported_subgroups) {
      for (regmatch_t &m : matches)
        m = {-1, -1};
      return REG_BADPAT;
    }
    if (want_pmatch) {
      LIBC_ASSERT(span.start <= span.end &&
                  span.end <= static_cast<size_t>(
                                  cpp::numeric_limits<regoff_t>::max()));
      matches.front() = {static_cast<regoff_t>(span.start),
                         static_cast<regoff_t>(span.end)};
      for (regmatch_t &m : matches.subspan(1))
        m = {-1, -1};
    }
    return 0;
  case regex::MatchResult::NoMatch:
    return REG_NOMATCH;
  case regex::MatchResult::OutOfMemory:
    return REG_ESPACE;
  }
  LIBC_ASSERT(false);
  __builtin_unreachable();
}

} // namespace LIBC_NAMESPACE_DECL
