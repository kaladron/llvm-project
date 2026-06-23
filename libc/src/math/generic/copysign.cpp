//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of copysign function.
///
//===----------------------------------------------------------------------===//

#include "src/math/copysign.h"
#include "src/__support/macros/properties/types.h"
#include "src/__support/math/copysign.h"

namespace LIBC_NAMESPACE_DECL {

/// Copy the sign of one float value to another.
///
/// \param x The value to get the magnitude from.
/// \param y The value to get the sign from.
/// \return A value with the magnitude of x and the sign of y.
LLVM_LIBC_FUNCTION(double, copysign, (double x, double y)) {
  return math::copysign(x, y);
}

} // namespace LIBC_NAMESPACE_DECL

#if defined(LIBC_ALIAS_LONG_DOUBLE_TO_DOUBLE)
#include "src/math/copysignl.h"

LLVM_LIBC_ALIAS(copysignl, copysign);

#endif // LIBC_ALIAS_LONG_DOUBLE_TO_DOUBLE
