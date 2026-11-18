//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// RAII temporary database file helper for hermetic netdb unit tests.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TEST_SRC_NETDB_TEST_DB_FILE_H
#define LLVM_LIBC_TEST_SRC_NETDB_TEST_DB_FILE_H

#include "src/__support/CPP/string_view.h"
#include "src/__support/File/file.h"
#include "src/stdio/remove.h"
#include "src/string/string_utils.h"

class ScopedTestDbFile {
  char path[256];
  void (*setter)(const char *);

public:
  ScopedTestDbFile(const char *file_path,
                   LIBC_NAMESPACE::cpp::string_view content,
                   void (*set_fn)(const char *))
      : setter(set_fn) {
    LIBC_NAMESPACE::internal::strlcpy(path, file_path, sizeof(path));

    LIBC_NAMESPACE::File *file = LIBC_NAMESPACE::openfile(path, "w").value();
    file->write(content.data(), content.size());
    file->close();

    setter(path);
  }

  ~ScopedTestDbFile() {
    setter(nullptr);
    LIBC_NAMESPACE::remove(path);
  }

  const char *get_path() const { return path; }
};

#endif // LLVM_LIBC_TEST_SRC_NETDB_TEST_DB_FILE_H
