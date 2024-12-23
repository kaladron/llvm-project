//===-- Implementation of shared ftw/nftw logic ---------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/ftw/ftw_impl.h"

#include "src/__support/CPP/string.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/File/scoped_dir.h"
#include "src/__support/libc_errno.h"
#include "src/fcntl/open.h"
#include "src/sys/stat/lstat.h"
#include "src/sys/stat/stat.h"
#include "src/unistd/close.h"

#include <dirent.h>
#include <ftw.h>
#include <sys/stat.h>

namespace LIBC_NAMESPACE_DECL {
namespace ftw_impl {

int doMergedFtw(const cpp::string &dirPath, const CallbackWrapper &fn,
                int fdLimit, int flags, int level) {
  // fdLimit specifies the maximum number of directories that ftw()
  // will hold open simultaneously. When a directory is opened, fdLimit is
  // decreased and if it becomes 0 or less, we won't open any more directories.
  if (fdLimit <= 0) {
    return 0;
  }

  // Determine the type of path that is passed.
  int typeFlag = FTW_F; // Default to regular file
  struct stat statBuf;
  if (flags & FTW_PHYS) {
    if (LIBC_NAMESPACE::lstat(dirPath.c_str(), &statBuf) < 0)
      return -1;
  } else {
    if (LIBC_NAMESPACE::stat(dirPath.c_str(), &statBuf) < 0) {
      if (LIBC_NAMESPACE::lstat(dirPath.c_str(), &statBuf) == 0) {
        typeFlag = FTW_SLN; /* Symbolic link pointing to a nonexistent file. */
      } else if (libc_errno != EACCES) {
        /* stat failed with an error that is not Permission denied */
        return -1;
      } else {
        /* The probable cause for the failure is that the caller had read
         * permission on the parent directory, so that the filename fpath could
         * be seen, but did not have execute permission on the directory.
         */
        typeFlag = FTW_NS;
      }
    }
  }

  // Determine type based on stat result (unless we already set a special type)
  if (typeFlag != FTW_SLN && typeFlag != FTW_NS) {
    if (S_ISDIR(statBuf.st_mode)) {
      if (flags & FTW_DEPTH) {
        typeFlag = FTW_DP; /* Directory, all subdirs have been visited. */
      } else {
        typeFlag = FTW_D; /* Directory. */
      }
    } else if (S_ISLNK(statBuf.st_mode)) {
      if (flags & FTW_PHYS) {
        typeFlag = FTW_SL; /* Symbolic link.  */
      } else {
        typeFlag = FTW_SLN; /* Symbolic link pointing to a nonexistent file. */
      }
    } else {
      typeFlag = FTW_F; /* Regular file.  */
    }
  }

  struct FTW ftwBuf;
  // Find the base by finding the last slash.
  cpp::string_view pathView(dirPath);
  size_t slash_found = pathView.find_last_of('/');
  if (slash_found != cpp::string_view::npos) {
    ftwBuf.base = static_cast<int>(slash_found + 1);
  } else {
    ftwBuf.base = 0;
  }

  ftwBuf.level = level;

  // If the dirPath is a file or symlink, call the function on it and return.
  if ((typeFlag == FTW_SL) || (typeFlag == FTW_F) || (typeFlag == FTW_SLN) ||
      (typeFlag == FTW_NS)) {
    return fn.call(dirPath.c_str(), &statBuf, typeFlag, &ftwBuf);
  }

  // If FTW_DEPTH is not set, nftw() shall report any directory before reporting
  // the files in that directory.
  if (!(flags & FTW_DEPTH)) {
    // Check if directory is readable
    int directory_fd = open(dirPath.c_str(), O_RDONLY);
    if (directory_fd < 0 && libc_errno == EACCES) {
      typeFlag = FTW_DNR; /* Directory can't be read. */
    }
    if (directory_fd >= 0)
      close(directory_fd);

    int returnValue = fn.call(dirPath.c_str(), &statBuf, typeFlag, &ftwBuf);
    if (returnValue) {
      return returnValue;
    }
  }

  // Open the directory for iteration
  auto dir_result = Dir::open(dirPath.c_str());
  if (!dir_result) {
    // Could not open directory - this might be FTW_DNR case, already handled
    // above
    libc_errno = dir_result.error();
    return -1;
  }
  ScopedDir dir(dir_result.value());

  // Iterate through directory entries
  while (true) {
    auto entry = dir->read();
    if (!entry) {
      // Error reading directory
      libc_errno = entry.error();
      return -1;
    }

    struct ::dirent *dirent_ptr = entry.value();
    if (dirent_ptr == nullptr) {
      // End of directory
      break;
    }

    // Skip "." and ".." entries
    if (dirent_ptr->d_name[0] == '.') {
      if (dirent_ptr->d_name[1] == '\0')
        continue;
      if (dirent_ptr->d_name[1] == '.' && dirent_ptr->d_name[2] == '\0')
        continue;
    }

    // Build the full path for the entry
    cpp::string entry_path = dirPath;
    if (!entry_path.empty() && entry_path[entry_path.size() - 1] != '/')
      entry_path += "/";
    entry_path += dirent_ptr->d_name;

    int return_value =
        doMergedFtw(entry_path, fn, fdLimit - 1, flags, ftwBuf.level + 1);
    if (return_value) {
      return return_value;
    }
  }

  // If FTW_DEPTH is set, nftw() shall report all files in a directory before
  // reporting the directory itself.
  if (flags & FTW_DEPTH) {
    // Call the function on the directory.
    return fn.call(dirPath.c_str(), &statBuf, typeFlag, &ftwBuf);
  }
  return 0;
}

} // namespace ftw_impl
} // namespace LIBC_NAMESPACE_DECL
