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
#include "hdr/types/size_t.h"
#include "hdr/types/struct_group.h"
#include "src/__support/CPP/span.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/error_or.h"
#include "src/__support/flat_file_db/flat_file_db.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace flat_file_db {

template <>
ErrorOr<void> parse_line<struct group>(cpp::span<char> line,
                                       cpp::span<char> scratch,
                                       struct group *grp);

} // namespace flat_file_db

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

// Searches for a group entry matching the given name. Both the strings and the
// gr_mem array are placed in the caller's buffer, so a buffer too small for the
// record yields ERANGE.
// The optional path parameter allows unit tests to direct lookups to hermetic
// test database files without mutating global state.
ErrorOr<bool> find_by_name(cpp::string_view name, struct group *grp,
                           cpp::span<char> buffer, const char *path = nullptr);

// Searches for a group entry matching the given group ID. See find_by_name for
// the buffer requirements.
ErrorOr<bool> find_by_gid(gid_t gid, struct group *grp, cpp::span<char> buffer,
                          const char *path = nullptr);

// Searches for a group entry matching the given name using the static
// process-global buffer.
ErrorOr<struct group *> find_by_name(cpp::string_view name);

// Searches for a group entry matching the given group ID using the static
// process-global buffer.
ErrorOr<struct group *> find_by_gid(gid_t gid);

} // namespace grp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_GRP_GRP_UTILS_H
