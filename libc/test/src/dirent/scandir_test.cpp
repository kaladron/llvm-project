//===-- Unittests for scandir ---------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/__support/CPP/string_view.h"
#include "src/dirent/scandir.h"

#include "test/UnitTest/ErrnoCheckingTest.h"
#include "test/UnitTest/Test.h"

#include <dirent.h>
#include <stdlib.h>

using LlvmLibcScandirTest = LIBC_NAMESPACE::testing::ErrnoCheckingTest;
using string_view = LIBC_NAMESPACE::cpp::string_view;

static int filter_all(const struct ::dirent *) { return 1; }

static int filter_files(const struct ::dirent *d) {
  string_view name(&d->d_name[0]);
  return name.starts_with("file");
}

static int compare_names(const struct ::dirent **a,
                         const struct ::dirent **b) {
  return string_view(&(*a)->d_name[0])
      .compare(string_view(&(*b)->d_name[0]));
}

TEST_F(LlvmLibcScandirTest, ScanAll) {
  struct ::dirent **namelist;
  int n = LIBC_NAMESPACE::scandir("testdata", &namelist, filter_all,
                                  nullptr);
  ASSERT_GT(n, 0);

  bool found_file1 = false, found_file2 = false;
  bool found_dir1 = false, found_dir2 = false;
  for (int i = 0; i < n; ++i) {
    string_view name(&namelist[i]->d_name[0]);
    if (name == "file1.txt")
      found_file1 = true;
    if (name == "file2.txt")
      found_file2 = true;
    if (name == "dir1")
      found_dir1 = true;
    if (name == "dir2")
      found_dir2 = true;
    ::free(namelist[i]);
  }
  ::free(namelist);

  ASSERT_TRUE(found_file1);
  ASSERT_TRUE(found_file2);
  ASSERT_TRUE(found_dir1);
  ASSERT_TRUE(found_dir2);
}

TEST_F(LlvmLibcScandirTest, ScanFiles) {
  struct ::dirent **namelist;
  int n = LIBC_NAMESPACE::scandir("testdata", &namelist, filter_files,
                                  nullptr);
  ASSERT_GT(n, 0);

  bool found_file1 = false, found_file2 = false;
  bool found_dir1 = false, found_dir2 = false;
  for (int i = 0; i < n; ++i) {
    string_view name(&namelist[i]->d_name[0]);
    if (name == "file1.txt")
      found_file1 = true;
    if (name == "file2.txt")
      found_file2 = true;
    if (name == "dir1")
      found_dir1 = true;
    if (name == "dir2")
      found_dir2 = true;
    ::free(namelist[i]);
  }
  ::free(namelist);

  ASSERT_TRUE(found_file1);
  ASSERT_TRUE(found_file2);
  ASSERT_FALSE(found_dir1);
  ASSERT_FALSE(found_dir2);
}

TEST_F(LlvmLibcScandirTest, ScanSorted) {
  struct ::dirent **namelist;
  int n = LIBC_NAMESPACE::scandir("testdata", &namelist, filter_all,
                                  compare_names);
  ASSERT_GT(n, 0);

  // Verify entries are sorted in ascending order.
  for (int i = 0; i < n - 1; ++i) {
    string_view name1(&namelist[i]->d_name[0]);
    string_view name2(&namelist[i + 1]->d_name[0]);
    ASSERT_LE(name1.compare(name2), 0);
  }

  for (int i = 0; i < n; ++i)
    ::free(namelist[i]);
  ::free(namelist);
}

TEST_F(LlvmLibcScandirTest, ScanNonExistentDir) {
  struct ::dirent **namelist;
  int n = LIBC_NAMESPACE::scandir("___xyz123__.non_existent__",
                                  &namelist, nullptr, nullptr);
  ASSERT_EQ(n, -1);
  ASSERT_ERRNO_EQ(ENOENT);
}

TEST_F(LlvmLibcScandirTest, ScanNullFilter) {
  struct ::dirent **namelist;
  int n = LIBC_NAMESPACE::scandir("testdata", &namelist, nullptr,
                                  nullptr);
  ASSERT_GT(n, 0);

  for (int i = 0; i < n; ++i)
    ::free(namelist[i]);
  ::free(namelist);
}
