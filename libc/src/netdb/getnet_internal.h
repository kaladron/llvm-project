//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Internal helpers for /etc/networks lookups.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC_NETDB_GETNET_INTERNAL_H
#define LLVM_LIBC_SRC_NETDB_GETNET_INTERNAL_H

#include "hdr/types/struct_netent.h"
#include "src/__support/CPP/span.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace netdb {

#ifndef LIBC_COPT_PUBLIC_PACKAGING
LIBC_INLINE_VAR LIBC_THREAD_LOCAL const char *networks_path = nullptr;

LIBC_INLINE void set_networks_path(const char *path) { networks_path = path; }

LIBC_INLINE const char *get_networks_path() { return networks_path; }
#else
LIBC_ALWAYS_INLINE constexpr const char *get_networks_path() { return nullptr; }
#endif // LIBC_COPT_PUBLIC_PACKAGING

int lookup_net_by_name(cpp::string_view name, struct netent *result_buf,
                       cpp::span<char> buffer, struct netent **result,
                       int *h_errnop, const char *path = nullptr);

} // namespace netdb
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_NETDB_GETNET_INTERNAL_H
