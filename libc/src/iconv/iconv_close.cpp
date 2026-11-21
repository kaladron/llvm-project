//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of iconv_close.
///
//===----------------------------------------------------------------------===//

#include "src/iconv/iconv_close.h"

#include "hdr/errno_macros.h"
#include "hdr/types/iconv_t.h"
#include "src/__support/CPP/unique_ptr.h"
#include "src/__support/common.h"
#include "src/__support/iconv/descriptor.h"
#include "src/__support/libc_errno.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(int, iconv_close, (iconv_t cd)) {
  if (internal::is_invalid_descriptor(cd)) {
    libc_errno = EBADF;
    return -1;
  }

  cpp::unique_ptr<internal::IconvDescriptor> owner(
      reinterpret_cast<internal::IconvDescriptor *>(cd));
  return 0;
}

} // namespace LIBC_NAMESPACE_DECL
