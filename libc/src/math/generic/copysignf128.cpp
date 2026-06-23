//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of copysignf128 function.
///
//===----------------------------------------------------------------------===//

#include "src/math/copysignf128.h"
#include "src/__support/macros/properties/types.h"
#include "src/__support/math/copysignf128.h"

namespace LIBC_NAMESPACE_DECL {

/// Copy the sign of one float value to another.
///
/// \param x The value to get the magnitude from.
/// \param y The value to get the sign from.
/// \return A value with the magnitude of x and the sign of y.
LLVM_LIBC_FUNCTION(float128, copysignf128, (float128 x, float128 y)) {
  return math::copysignf128(x, y);
}

} // namespace LIBC_NAMESPACE_DECL

#if defined(LIBC_ALIAS_LONG_DOUBLE_TO_FLOAT128)
#include "src/math/copysignl.h"

LLVM_LIBC_ALIAS(copysignl, copysignf128);

#endif // LIBC_ALIAS_LONG_DOUBLE_TO_FLOAT128
