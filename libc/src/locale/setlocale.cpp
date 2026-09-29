//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of setlocale.
///
//===----------------------------------------------------------------------===//

#include "src/locale/setlocale.h"
#include "hdr/locale_macros.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/locale/locale.h"
#include "src/locale/locale_data.h"

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(char *, setlocale, (int category, const char *locale_name)) {
  if (category < LC_CTYPE || category > LC_ALL)
    return nullptr;

  locale_t effective_loc = nullptr;
  if constexpr (DISABLE_RUNTIME_LOCALE) {
    if (!locale_name) {
      effective_loc = DEFAULT_LOCALE_IS_UTF8 ? &utf8_locale : &c_locale;
    } else {
      effective_loc = resolve_locale(LC_ALL_MASK, locale_name);
      if (!effective_loc)
        return nullptr;
    }
  } else {
    if (!locale_name) {
      effective_loc =
          (category == LC_ALL || category == LC_CTYPE) ? global_locale : &c_locale;
    } else {
      int mask = (category == LC_ALL) ? LC_ALL_MASK : (1 << category);
      effective_loc = resolve_locale(mask, locale_name);
      if (!effective_loc)
        return nullptr;
      if (category == LC_ALL || category == LC_CTYPE)
        global_locale = effective_loc;
    }
  }

  // POSIX requires a char * return type, though callers may not modify it.
  return const_cast<char *>(effective_loc == &utf8_locale ? "C.UTF-8" : "C");
}

} // namespace LIBC_NAMESPACE_DECL
