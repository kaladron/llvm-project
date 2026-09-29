//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of newlocale.
///
//===----------------------------------------------------------------------===//

#include "src/locale/newlocale.h"
#include "hdr/locale_macros.h"
#include "src/__support/common.h"
#include "src/__support/libc_errno.h"
#include "src/__support/macros/config.h"
#include "src/locale/locale.h"
#include "src/locale/locale_data.h"

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(locale_t, newlocale,
                   (int category_mask, const char *locale_name,
                    locale_t base)) {
  if (!locale_name || (category_mask & ~LC_ALL_MASK) != 0) {
    libc_errno = EINVAL;
    return nullptr;
  }

  locale_t loc = resolve_locale(category_mask, locale_name);
  if (!loc) {
    libc_errno = ENOENT;
    return nullptr;
  }

  if (!(category_mask & LC_CTYPE_MASK))
    return base ? base : (DEFAULT_LOCALE_IS_UTF8 ? &utf8_locale : &c_locale);

  return loc;
}

} // namespace LIBC_NAMESPACE_DECL
