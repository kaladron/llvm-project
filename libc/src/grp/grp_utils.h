//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Declarations of helper functions and parser for grp.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC_GRP_GRP_UTILS_H
#define LLVM_LIBC_SRC_GRP_GRP_UTILS_H

#include "hdr/types/gid_t.h"
#include "hdr/types/struct_group.h"
#include "src/__support/CPP/span.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace grp {

// Parses a colon-separated group line into a struct group, filling mem_ptrs
// with null-terminated member pointers.
bool parse_group_line(cpp::span<char> line, struct group *grp,
                      cpp::span<char *> mem_ptrs);

void TESTONLY_set_group_path(const char *path);
void TESTONLY_reset_group_path();

// Opens or rewinds the group database stream.
ErrorOr<void> open();

// Closes the group database stream.
ErrorOr<void> close();

// Reads the next entry from the group database.
ErrorOr<struct group *> read_next();

} // namespace grp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_GRP_GRP_UTILS_H
