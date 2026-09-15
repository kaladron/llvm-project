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

#include "hdr/types/size_t.h"
#include "hdr/types/struct_group.h"
#include "src/__support/CPP/span.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include "src/__support/pwd/flat_file_db.h"

namespace LIBC_NAMESPACE_DECL {
namespace pwd {

template <>
ErrorOr<void> parse_line<struct group>(cpp::span<char> line,
                                       cpp::span<char> scratch,
                                       struct group *grp);

} // namespace pwd

namespace grp {

// Parses a colon-separated group line into a struct group, filling mem_ptrs
// with null-terminated member pointers.
bool parse_group_line(cpp::span<char> line, struct group *grp,
                      cpp::span<char *> mem_ptrs);

// Overrides the group database path for testing, closing any open stream and
// releasing the growable buffer.
void TESTONLY_set_group_path(const char *path);

// Resets the group database path to the default, closing any open stream and
// releasing the growable buffer.
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
