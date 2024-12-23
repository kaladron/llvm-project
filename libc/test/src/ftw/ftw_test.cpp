//===-- Unittests for ftw and nftw ----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/__support/CPP/string_view.h"
#include "src/dirent/closedir.h"
#include "src/dirent/opendir.h"
#include "src/dirent/readdir.h"
#include "src/fcntl/open.h"
#include "src/ftw/ftw.h"
#include "src/ftw/nftw.h"
#include "src/sys/stat/mkdir.h"
#include "src/unistd/close.h"
#include "src/unistd/rmdir.h"
#include "src/unistd/symlink.h"
#include "src/unistd/unlink.h"
#include "test/UnitTest/ErrnoCheckingTest.h"
#include "test/UnitTest/ErrnoSetterMatcher.h"
#include "test/UnitTest/Test.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <sys/stat.h>

using LlvmLibcFtwTest = LIBC_NAMESPACE::testing::ErrnoCheckingTest;
using LlvmLibcNftwTest = LIBC_NAMESPACE::testing::ErrnoCheckingTest;
using LIBC_NAMESPACE::cpp::string_view;

// Test data structure to track visited files
struct VisitedFiles {
  static constexpr int MAX_FILES = 32;
  char paths[MAX_FILES][256];
  int types[MAX_FILES];
  int levels[MAX_FILES];
  int count;

  void reset() { count = 0; }

  void add(const char *path, int type, int level) {
    if (count < MAX_FILES) {
      // Copy path manually (no strncpy in freestanding)
      int i = 0;
      while (path[i] && i < 255) {
        paths[count][i] = path[i];
        i++;
      }
      paths[count][i] = '\0';
      types[count] = type;
      levels[count] = level;
      count++;
    }
  }

  bool contains(const char *substring) const {
    string_view sub(substring);
    for (int i = 0; i < count; i++) {
      string_view path(paths[i]);
      if (path.find_first_of(sub[0]) != string_view::npos) {
        // Simple substring check
        for (size_t j = 0; j <= path.size() - sub.size(); j++) {
          bool match = true;
          for (size_t k = 0; k < sub.size() && match; k++) {
            if (paths[i][j + k] != substring[k])
              match = false;
          }
          if (match)
            return true;
        }
      }
    }
    return false;
  }

  int getTypeFor(const char *substring) const {
    string_view sub(substring);
    for (int i = 0; i < count; i++) {
      string_view path(paths[i]);
      for (size_t j = 0; j + sub.size() <= path.size(); j++) {
        bool match = true;
        for (size_t k = 0; k < sub.size() && match; k++) {
          if (paths[i][j + k] != substring[k])
            match = false;
        }
        if (match)
          return types[i];
      }
    }
    return -1;
  }
};

static VisitedFiles gVisited;

// Callback for nftw that records visited files
static int recordVisit(const char *fpath, const struct stat *sb, int typeflag,
                       struct FTW *ftwbuf) {
  (void)sb; // unused
  gVisited.add(fpath, typeflag, ftwbuf->level);
  return 0; // continue traversal
}

// Callback for ftw that records visited files
static int recordVisitFtw(const char *fpath, const struct stat *sb,
                          int typeflag) {
  (void)sb; // unused
  gVisited.add(fpath, typeflag, 0);
  return 0; // continue traversal
}

// Callback that stops after finding a specific file
static int stopOnFile(const char *fpath, const struct stat *sb, int typeflag,
                      struct FTW *ftwbuf) {
  (void)sb;
  (void)ftwbuf;
  gVisited.add(fpath, typeflag, 0);
  // Check if path contains "stopfile"
  string_view path(fpath);
  for (size_t i = 0; i + 8 <= path.size(); i++) {
    if (fpath[i] == 's' && fpath[i + 1] == 't' && fpath[i + 2] == 'o' &&
        fpath[i + 3] == 'p' && fpath[i + 4] == 'f' && fpath[i + 5] == 'i' &&
        fpath[i + 6] == 'l' && fpath[i + 7] == 'e')
      return 1; // stop traversal
  }
  return 0;
}

// Helper to create a file
static bool createFile(const char *path) {
  int fd = LIBC_NAMESPACE::open(path, O_CREAT | O_WRONLY, S_IRWXU);
  if (fd < 0)
    return false;
  LIBC_NAMESPACE::close(fd);
  return true;
}

// Helper to create a directory
static bool createDir(const char *path) {
  return LIBC_NAMESPACE::mkdir(path, S_IRWXU) == 0;
}

// Simplest callback that does nothing
static int simpleCallback(const char *fpath, const struct stat *sb,
                          int typeflag) {
  (void)fpath;
  (void)sb;
  (void)typeflag;
  return 0;
}

// Use static test directory that exists
TEST_F(LlvmLibcFtwTest, BasicTraversalWithTestData) {
  // First make sure testdata directory exists
  ::DIR *dir = LIBC_NAMESPACE::opendir("testdata");
  if (dir == nullptr) {
    // Skip test if testdata doesn't exist
    return;
  }
  LIBC_NAMESPACE::closedir(dir);

  int result = LIBC_NAMESPACE::ftw("testdata", simpleCallback, 10);
  ASSERT_EQ(result, 0);
}

TEST_F(LlvmLibcFtwTest, NonexistentPath) {
  gVisited.reset();
  int result = LIBC_NAMESPACE::ftw("/nonexistent/path", recordVisitFtw, 10);
  EXPECT_EQ(result, -1);
  ASSERT_ERRNO_EQ(ENOENT);
}

TEST_F(LlvmLibcNftwTest, BasicTraversalWithTestData) {
  gVisited.reset();
  int result = LIBC_NAMESPACE::nftw("testdata", recordVisit, 10, 0);
  ASSERT_EQ(result, 0);

  // Should have visited some files
  EXPECT_GE(gVisited.count, 1);
}

TEST_F(LlvmLibcNftwTest, NonexistentPath) {
  gVisited.reset();
  int result = LIBC_NAMESPACE::nftw("/nonexistent/path/that/does/not/exist",
                                    recordVisit, 10, 0);
  EXPECT_EQ(result, -1);
  ASSERT_ERRNO_EQ(ENOENT);
}

TEST_F(LlvmLibcNftwTest, DepthFirstFlag) {
  gVisited.reset();
  int result = LIBC_NAMESPACE::nftw("testdata", recordVisit, 10, FTW_DEPTH);
  ASSERT_EQ(result, 0);

  // Should have visited files
  EXPECT_GE(gVisited.count, 1);
}

TEST_F(LlvmLibcNftwTest, PhysicalFlag) {
  gVisited.reset();
  int result = LIBC_NAMESPACE::nftw("testdata", recordVisit, 10, FTW_PHYS);
  ASSERT_EQ(result, 0);

  // Should have visited files
  EXPECT_GE(gVisited.count, 1);
}

TEST_F(LlvmLibcNftwTest, CallbackCanStopTraversal) {
  gVisited.reset();
  // Use a callback that returns non-zero
  auto stopImmediately = [](const char *, const struct stat *, int,
                            struct FTW *) -> int { return 42; };
  int result = LIBC_NAMESPACE::nftw("testdata", stopImmediately, 10, 0);
  // nftw should return the callback's return value
  EXPECT_EQ(result, 42);
}
