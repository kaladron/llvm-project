//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of helper functions and parser for grp.
///
//===----------------------------------------------------------------------===//

#include "src/grp/grp_utils.h"
#include "hdr/types/gid_t.h"
#include "hdr/types/size_t.h"
#include "hdr/types/struct_group.h"
#include "src/__support/CPP/span.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/ctype_utils.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/pwd/field_tokenizer.h"
#include "src/__support/pwd/flat_file_db.h"
#include "src/__support/str_to_integer.h"

namespace LIBC_NAMESPACE_DECL {

static constexpr size_t MAX_MEMBERS = 512;
static constexpr size_t LINE_BUFFER_SIZE = 1024;
static char *member_pointers[MAX_MEMBERS];
static char line_buffer[LINE_BUFFER_SIZE];
static struct group grp_entry;

#ifndef LIBC_COPT_GROUP_FILE_PATH
#define LIBC_COPT_GROUP_FILE_PATH "/etc/group"
#endif

LIBC_CONSTINIT static pwd::FlatFileDatabase<struct group>
    db(LIBC_COPT_GROUP_FILE_PATH);

namespace pwd {
template <>
LIBC_INLINE bool parse_line<struct group>(cpp::span<char> line,
                                          struct group *grp) {
  return grp::parse_group_line(line, grp, member_pointers);
}
} // namespace pwd

namespace grp {

bool parse_group_line(cpp::span<char> line, struct group *grp,
                      cpp::span<char *> mem_ptrs) {
  if (line.empty() || !grp || mem_ptrs.empty())
    return false;

  pwd::FieldTokenizer tokenizer(line, ':');

  auto name = tokenizer.next_field();
  if (!name || name->empty() || name->front() == '\0')
    return false;
  grp->gr_name = name->data();

  auto passwd = tokenizer.next_field();
  if (!passwd)
    return false;
  grp->gr_passwd = passwd->data();

  auto gid_str = tokenizer.next_field();
  if (!gid_str || gid_str->empty() || !internal::isdigit(gid_str->front()))
    return false;
  auto gid_res = internal::strtointeger<gid_t>(gid_str->data(), 10);
  if (gid_res.has_error() || gid_res.parsed_len <= 0 ||
      static_cast<size_t>(gid_res.parsed_len) >= gid_str->size() ||
      (*gid_str)[gid_res.parsed_len] != '\0')
    return false;
  grp->gr_gid = gid_res.value;

  auto members_field = tokenizer.next_field();
  if (!members_field)
    return false;

  // Trailing delimiters or fields are invalid.
  if (tokenizer.next_field())
    return false;

  size_t member_count = 0;
  if (!members_field->empty() && members_field->front() != '\0') {
    pwd::FieldTokenizer member_tokenizer(*members_field, ',');
    while (auto member = member_tokenizer.next_field()) {
      if (member->empty() || member->front() == '\0')
        continue;
      if (member_count + 1 >= mem_ptrs.size())
        return false;
      mem_ptrs[member_count++] = member->data();
    }
  }

  if (member_count >= mem_ptrs.size())
    return false;
  mem_ptrs[member_count] = nullptr;
  grp->gr_mem = mem_ptrs.data();

  return true;
}

void TESTONLY_set_group_path(const char *path) { db.set_path(path); }

void TESTONLY_reset_group_path() { db.set_path(LIBC_COPT_GROUP_FILE_PATH); }

ErrorOr<void> open() { return db.setdb(); }

ErrorOr<void> close() { return db.enddb(); }

ErrorOr<struct group *> read_next() {
  auto res = db.getnext(&grp_entry, line_buffer);
  if (!res.has_value())
    return Error(res.error());
  if (!res.value())
    return nullptr;
  return &grp_entry;
}

} // namespace grp
} // namespace LIBC_NAMESPACE_DECL
