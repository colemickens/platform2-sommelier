// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "init/file_attrs_cleaner.h"

#include <dirent.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/xattr.h>
#include <unistd.h>

#include <linux/fs.h>

#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/files/scoped_file.h>
#include <base/logging.h>

namespace file_attrs_cleaner {

const char xdg_origin_url[] = "user.xdg.origin.url";
const char xdg_referrer_url[] = "user.xdg.referrer.url";

namespace {

// Paths that we allow to be made immutable.
const char permitted_immutable_dirs[] = {
    // We mark this immutable as we use it with a lot of daemons to pivot into
    // when using minijail and setting up a reduced mount namespace.
    "/var/empty",
};

struct ScopedDirDeleter {
  inline void operator()(DIR* dirp) const {
    if (dirp)
      closedir(dirp);
  }
};
using ScopedDir = std::unique_ptr<DIR, ScopedDirDeleter>;

}  // namespace

bool ImmutableAllowed(const base::FilePath& path, bool isdir) {
  if (!isdir) {
    // We don't allow immutable on any non-directories (yet?).
    return false;
  }

  for (size_t i = 0; i < arraysize(permitted_immutable_dirs); ++i) {
    if (path.value() == &permitted_immutable_dirs[i])
      return true;
  }
  return false;
}

bool CheckFileAttributes(const base::FilePath& path, bool isdir, int fd) {
  long flags;  // NOLINT(runtime/int)
  if (ioctl(fd, FS_IOC_GETFLAGS, &flags) != 0) {
    PLOG(WARNING) << "Getting flags on " << path.value() << " failed";
    return false;
  }

  if (flags & FS_IMMUTABLE_FL) {
    if (!ImmutableAllowed(path, isdir)) {
      LOG(WARNING) << "Immutable bit found on " << path.value()
                   << "; clearing it";
      flags &= ~FS_IMMUTABLE_FL;
      if (ioctl(fd, FS_IOC_SETFLAGS, &flags) != 0) {
        PLOG(ERROR) << "Unable to clear immutable bit on " << path.value();
        return false;
      }
    }
  }

  // The other file attribute flags look benign at this point.
  return true;
}

bool RemoveURLExtendedAttributes(const base::FilePath& path) {
  bool xattr_success = true;
  for (const auto& attr_name : {xdg_origin_url, xdg_referrer_url}) {
    if (getxattr(path.value().c_str(), attr_name, nullptr, 0) >= 0) {
      // Attribute exists, clear it.
      bool res = removexattr(path.value().c_str(), attr_name) == 0;
      if (!res) {
        PLOG(ERROR) << "Unable to remove extended attribute '" << attr_name
                    << "' from " << path.value();
      }
      xattr_success &= res;
    }
  }

  return xattr_success;
}

bool ScanDir(const base::FilePath& dir) {
  // Internally glibc will use O_CLOEXEC when opening the directory.
  // Unfortunately, there is no opendirat() helper we could use (so that ScanDir
  // could accept a fd argument).
  //
  // We could use openat() ourselves and pass that to fdopendir(), but that has
  // two downsides: (1) We can't use ScopedFD because opendir() will take over
  // the fd -- when closedir() is called, close() will also be called.  We can't
  // skip the closedir() because we need to let the C library release resources
  // associated with the DIR* handle.  (2) When using fdopendir(), glibc will
  // use fcntl() to make sure O_CLOEXEC is set even if we set it ourselves when
  // we called openat().  It works, but adds a bit of syscall overhead here.
  // Along those lines, we could dup() the fd passed in, but that would also add
  // syscall overhead with no real benefit.
  //
  // So unless we change the signature of ScanDir to take a fd of the open dir
  // to scan, we stick with opendir() here.  Since this program only runs during
  // early OS init, there shouldn't be other programs in the system racing with
  // us to cause problems.
  ScopedDir dirp(opendir(dir.value().c_str()));
  if (dirp.get() == nullptr) {
    PLOG(WARNING) << "Unable to open directory " << dir.value();
    return false;
  }

  int dfd = dirfd(dirp.get());
  if (!CheckFileAttributes(dir, true, dfd)) {
    // This should never really fail ...
    return false;
  }

  // We might need this if we descend into a subdir.  But if it's a leaf
  // directory (no subdirs), we can skip the stat overhead entirely.
  bool have_dirst = false;
  struct stat dirst;

  // Scan all the entries in this directory.
  bool ret = true;
  struct dirent* de;
  while ((de = readdir(dirp.get())) != nullptr) {
    CHECK(de->d_type != DT_UNKNOWN);

    // Skip symlinks.
    if (de->d_type == DT_LNK)
      continue;

    // Skip the . and .. fake directories.
    const std::string_view name(de->d_name);
    if (name == "." || name == "..")
      continue;

    const base::FilePath path = dir.Append(de->d_name);
    if (de->d_type == DT_DIR) {
      // Don't cross mountpoints.
      if (!have_dirst) {
        // Load this on demand so leaf dirs don't waste time.
        have_dirst = true;
        if (fstat(dfd, &dirst) != 0) {
          PLOG(ERROR) << "Unable to stat " << dir.value();
          ret = false;
          continue;
        }
      }

      struct stat subdirst;
      if (fstatat(dfd, de->d_name, &subdirst, 0) != 0) {
        PLOG(ERROR) << "Unable to stat " << path.value();
        ret = false;
        continue;
      }

      if (dirst.st_dev != subdirst.st_dev) {
        DVLOG(1) << "Skipping mounted directory " << path.value();
        continue;
      }

      // Descend into this directory.
      ret &= ScanDir(path);
    } else if (de->d_type == DT_REG) {
      // Check the settings on this file.

      // Extended attributes can be read even on encrypted files, so remove them
      // by path and not by file descriptor.
      // Since the removal is best-effort anyway, TOCTOU issues should not be
      // a problem.
      ret &= RemoveURLExtendedAttributes(path);

      base::ScopedFD fd(openat(dfd, de->d_name,
                               O_RDONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC));

      if (!fd.is_valid()) {
        // This is normal for encrypted files.
        if (errno == ENOKEY)
          continue;

        PLOG(ERROR) << "Skipping path: " << path.value();
        ret = false;
        continue;
      }

      ret &= CheckFileAttributes(path, false, fd.get());
    } else {
      LOG(WARNING) << "Skipping path: " << path.value() << ": unknown type "
                   << de->d_type;
    }
  }

  return ret;
}

}  // namespace file_attrs_cleaner
