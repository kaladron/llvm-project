//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of iconv_open.
///
//===----------------------------------------------------------------------===//

#include "src/iconv/iconv_open.h"

#include "hdr/errno_macros.h"
#include "hdr/types/iconv_t.h"
#include "src/__support/checked_ptr.h"
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/iconv/descriptor.h"
#include "src/__support/libc_errno.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/null_check.h"

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(iconv_t, iconv_open,
                   (const char *tocode, const char *fromcode)) {
  LIBC_CRASH_ON_NULLPTR(tocode);
  LIBC_CRASH_ON_NULLPTR(fromcode);

  ErrorOr<internal::IconvDescriptor> resolved =
      internal::resolve_descriptor(tocode, fromcode);
  if (!resolved.has_value()) {
    libc_errno = resolved.error();
    return internal::invalid_iconv_descriptor();
  }

  CheckedPtr<internal::IconvDescriptor> desc =
      make_unique<internal::IconvDescriptor>(*resolved);
  if (!desc) {
    libc_errno = ENOMEM;
    return internal::invalid_iconv_descriptor();
  }

  return reinterpret_cast<iconv_t>(desc.release());
}

} // namespace LIBC_NAMESPACE_DECL
