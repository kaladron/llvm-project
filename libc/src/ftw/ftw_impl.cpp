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
#include "src/unistd/chdir.h"
#include "src/unistd/close.h"
#include "src/unistd/fchdir.h"

#include "hdr/fcntl_macros.h"
#include "hdr/ftw_macros.h"
#include "hdr/sys_stat_macros.h"
#include "include/llvm-libc-types/struct_FTW.h"
#include "include/llvm-libc-types/struct_dirent.h"
#include "include/llvm-libc-types/struct_stat.h"

namespace LIBC_NAMESPACE_DECL {
namespace ftw_impl {

class StartDirSaver {
  int startFd;

public:
  StartDirSaver(int fd) : startFd(fd) {}
  ~StartDirSaver() {
    if (startFd >= 0) {
      LIBC_NAMESPACE::fchdir(startFd);
      LIBC_NAMESPACE::close(startFd);
    }
  }
};

class LevelDirSaver {
  bool active;

public:
  LevelDirSaver(bool doChdir) : active(doChdir) {}
  ~LevelDirSaver() {
    if (active)
      LIBC_NAMESPACE::chdir("..");
  }
};

cpp::expected<int, int>
doMergedFtw(const cpp::string &dirPath, const CallbackWrapper &fn, int fdLimit,
            int flags, int level, unsigned long startDevice,
            AncestorDir *ancestors) {
  int startFd = -1;
  if (level == 0 && (flags & FTW_CHDIR)) {
    startFd = LIBC_NAMESPACE::open(".", O_RDONLY);
    if (startFd < 0)
      return cpp::unexpected<int>(libc_errno);
  }
  StartDirSaver rootSaver(startFd);

  struct FTW ftwBuf;
  ftwBuf.level = level;
  cpp::string_view pathView(dirPath);
  size_t slashFound = pathView.find_last_of('/');
  ftwBuf.base = (slashFound != cpp::string_view::npos)
                    ? static_cast<int>(slashFound + 1)
                    : 0;

  const char *osPath = (level == 0 || !(flags & FTW_CHDIR))
                           ? dirPath.c_str()
                           : (dirPath.c_str() + ftwBuf.base);

  int typeFlag = FTW_F;
  struct stat statBuf;
  if (flags & FTW_PHYS) {
    if (LIBC_NAMESPACE::lstat(osPath, &statBuf) < 0) {
      if (libc_errno == EACCES)
        typeFlag = FTW_NS;
      else
        return cpp::unexpected<int>(libc_errno);
    }
  } else {
    if (LIBC_NAMESPACE::stat(osPath, &statBuf) < 0) {
      if (LIBC_NAMESPACE::lstat(osPath, &statBuf) == 0)
        typeFlag = FTW_SLN;
      else if (libc_errno != EACCES)
        return cpp::unexpected<int>(libc_errno);
      else
        typeFlag = FTW_NS;
    }
  }

  if ((flags & FTW_MOUNT) && level > 0 && statBuf.st_dev != startDevice)
    return 0;

  if (typeFlag != FTW_SLN && typeFlag != FTW_NS) {
    if (S_ISDIR(statBuf.st_mode))
      typeFlag = (flags & FTW_DEPTH) ? FTW_DP : FTW_D;
    else if (S_ISLNK(statBuf.st_mode))
      typeFlag = (flags & FTW_PHYS) ? FTW_SL : FTW_SLN;
    else
      typeFlag = FTW_F;
  }

  if (!fn.isNftw && typeFlag == FTW_SLN)
    typeFlag = FTW_SL;

  if (typeFlag == FTW_D || typeFlag == FTW_DP || typeFlag == FTW_DNR) {
    for (AncestorDir *a = ancestors; a != nullptr; a = a->parent) {
      if (a->dev == statBuf.st_dev && a->ino == statBuf.st_ino)
        return 0;
    }
  }

  if (typeFlag != FTW_D && typeFlag != FTW_DP) {
    int ret = fn.call(dirPath.c_str(), &statBuf, typeFlag, &ftwBuf);
    if (ret != 0) {
      if (flags & FTW_ACTIONRETVAL) {
        if (ret == FTW_SKIP_SUBTREE || ret == FTW_SKIP_SIBLINGS)
          return ret;
      }
      return ret;
    }
    return 0;
  }

  bool skipSubtree = false;
  if (!(flags & FTW_DEPTH)) {
    if (fdLimit > 0) {
      int dirFd = LIBC_NAMESPACE::open(osPath, O_RDONLY);
      if (dirFd < 0 && libc_errno == EACCES)
        typeFlag = FTW_DNR;
      else if (dirFd >= 0)
        LIBC_NAMESPACE::close(dirFd);
    }

    int ret = fn.call(dirPath.c_str(), &statBuf, typeFlag, &ftwBuf);
    if (ret != 0) {
      if (flags & FTW_ACTIONRETVAL) {
        if (ret == FTW_SKIP_SUBTREE) {
          skipSubtree = true;
        } else if (ret == FTW_SKIP_SIBLINGS) {
          return ret;
        } else {
          return ret;
        }
      } else {
        return ret;
      }
    }
  }

  if (typeFlag != FTW_DNR && !skipSubtree) {
    if (fdLimit <= 0)
      return cpp::unexpected<int>(EMFILE);

    auto dirResult = Dir::open(osPath);
    if (!dirResult) {
      if (dirResult.error() != EACCES)
        return cpp::unexpected<int>(dirResult.error());
    } else {
      ScopedDir dir(dirResult.value());
      if (flags & FTW_CHDIR) {
        if (LIBC_NAMESPACE::chdir(osPath) < 0)
          return cpp::unexpected<int>(libc_errno);
      }
      LevelDirSaver levelSaver(flags & FTW_CHDIR);
      AncestorDir currentAncestor = {statBuf.st_dev, statBuf.st_ino, ancestors};

      while (true) {
        auto entry = dir->read();
        if (!entry)
          return cpp::unexpected(entry.error());

        struct ::dirent *direntPtr = entry.value();
        if (direntPtr == nullptr)
          break;

        if (direntPtr->d_name[0] == '.') {
          if (direntPtr->d_name[1] == '\0' ||
              (direntPtr->d_name[1] == '.' && direntPtr->d_name[2] == '\0'))
            continue;
        }

        cpp::string entryPath = dirPath;
        if (!entryPath.empty() && entryPath[entryPath.size() - 1] != '/')
          entryPath += "/";
        entryPath += direntPtr->d_name;

        auto res = doMergedFtw(entryPath, fn, fdLimit - 1, flags, level + 1,
                               startDevice, &currentAncestor);
        if (!res)
          return res;
        if (flags & FTW_ACTIONRETVAL) {
          if (res.value() == FTW_SKIP_SIBLINGS)
            break;
          if (res.value() != 0 && res.value() != FTW_SKIP_SUBTREE)
            return res.value();
        } else if (res.value() != 0) {
          return res.value();
        }
      }
    }
  }

  if (flags & FTW_DEPTH) {
    int ret = fn.call(dirPath.c_str(), &statBuf, typeFlag, &ftwBuf);
    if (flags & FTW_ACTIONRETVAL) {
      if (ret == FTW_SKIP_SIBLINGS || ret == FTW_SKIP_SUBTREE)
        return ret;
    }
    return ret;
  }
  return 0;
}

} // namespace ftw_impl
} // namespace LIBC_NAMESPACE_DECL
