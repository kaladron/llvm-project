//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Internal OS network interface and address configuration helpers.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_NETWORK_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_NETWORK_H

#if defined(__linux__)
#include "src/__support/OSUtil/linux/network.h"
#else
#error network.h unavailable on this platform
#endif

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_NETWORK_H
