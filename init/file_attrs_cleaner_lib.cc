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

bool CheckSucceeded(AttributeCheckStatus status) {
  return status == AttributeCheckStatus::NO_ATTR ||
         status == AttributeCheckStatus::CLEARED;
}

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

AttributeCheckStatus CheckFileAttributes(const base::FilePath& path,
                                         bool isdir,
                                         int fd) {
  long flags;  // NOLINT(runtime/int)
  if (ioctl(fd, FS_IOC_GETFLAGS, &flags) != 0) {
    PLOG(WARNING) << "Getting flags on " << path.value() << " failed";
    return AttributeCheckStatus::ERROR;
  }

  if (flags & FS_IMMUTABLE_FL) {
    if (!ImmutableAllowed(path, isdir)) {
      LOG(WARNING) << "Immutable bit found on " << path.value()
                   << "; clearing it";
      flags &= ~FS_IMMUTABLE_FL;
      if (ioctl(fd, FS_IOC_SETFLAGS, &flags) != 0) {
        PLOG(ERROR) << "Unable to clear immutable bit on " << path.value();
        return AttributeCheckStatus::CLEAR_FAILED;
      }
    }
    return AttributeCheckStatus::CLEARED;
  }

  // The other file attribute flags look benign at this point.
  return AttributeCheckStatus::NO_ATTR;
}

AttributeCheckStatus RemoveURLExtendedAttributes(const base::FilePath& path) {
  bool found_xattr = false;
  bool xattr_success = true;

  for (const auto& attr_name : {xdg_origin_url, xdg_referrer_url}) {
    if (getxattr(path.value().c_str(), attr_name, nullptr, 0) >= 0) {
      // Attribute exists, clear it.
      found_xattr = true;
      bool res = removexattr(path.value().c_str(), attr_name) == 0;
      if (!res) {
        PLOG(ERROR) << "Unable to remove extended attribute '" << attr_name
                    << "' from " << path.value();
      }
      xattr_success &= res;
    }
  }

  if (found_xattr) {
    return xattr_success ? AttributeCheckStatus::CLEARED
                         : AttributeCheckStatus::CLEAR_FAILED;
  } else {
    return AttributeCheckStatus::NO_ATTR;
  }
}

bool ScanDir(const base::FilePath& dir,
             const std::vector<std::string>& skip_recurse,
             int* url_xattrs_count) {
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

  *url_xattrs_count = 0;

  ScopedDir dirp(opendir(dir.value().c_str()));
  if (dirp.get() == nullptr) {
    PLOG(WARNING) << "Unable to open directory " << dir.value();
    // This is a best effort routine so don't fail if the directory cannot be
    // opened.
    return true;
  }

  int dfd = dirfd(dirp.get());
  if (!CheckSucceeded(CheckFileAttributes(dir, true /*isdir*/, dfd))) {
    // This should never really fail...
    return false;
  }

  // We might need this if we descend into a subdir.  But if it's a leaf
  // directory (no subdirs), we can skip the stat overhead entirely.
  bool have_dirst = false;
  struct stat dirst;

  // Scan all the entries in this directory.
  bool ret = true;
  struct dirent* de;
  std::vector<base::FilePath> subdirs;
  while ((de = readdir(dirp.get())) != nullptr) {
    CHECK(de->d_type != DT_UNKNOWN);

    // Skip symlinks.
    if (de->d_type == DT_LNK)
      continue;

    // Skip the . and .. fake directories.
    const std::string_view name(de->d_name);
    if (name == "." || name == "..")
      continue;

    // If the path component is listed in |skip_recurse|, skip it.
    if (std::find(skip_recurse.begin(), skip_recurse.end(), name) !=
        skip_recurse.end())
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

      // Enqueue this directory for recursing.
      // Recursing here is problematic because it means that |dirp| remains
      // open for the lifetime of the process. Having a handle to the directory
      // open for that long causes problems if the tool is still running when
      // a user logs in. This can happen if the user has a lot of files in
      // their home directory.
      subdirs.push_back(path);
    } else if (de->d_type == DT_REG) {
      // Check the settings on this file.

      // Extended attributes can be read even on encrypted files, so remove
      // them by path and not by file descriptor. Since the removal is
      // best-effort anyway, TOCTOU issues should not be a problem.
      AttributeCheckStatus status = RemoveURLExtendedAttributes(path);
      ret &= CheckSucceeded(status);
      if (status == AttributeCheckStatus::CLEARED)
        ++(*url_xattrs_count);

      base::ScopedFD fd(openat(dfd, de->d_name,
                               O_RDONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC));

      if (!fd.is_valid()) {
        // This routine can be executed over encrypted filesystems.
        // ENOKEY is normal for encrypted files, so don't log in that case.
        if (errno != ENOKEY)
          PLOG(WARNING) << "Skipping path: " << path.value();

        // This is a best effort routine so don't fail if the path cannot be
        // opened.
        continue;
      }

      ret &=
          CheckSucceeded(CheckFileAttributes(path, false /*is_dir*/, fd.get()));

    } else {
      LOG(WARNING) << "Skipping path: " << path.value() << ": unknown type "
                   << de->d_type;
    }
  }

  if (closedir(dirp.release()) != 0)
    PLOG(ERROR) << "Unable to close directory " << dir.value();

  int sub_xattrs_count = 0;
  for (const auto& subdir : subdirs) {
    // Descend into this directory.
    if (ScanDir(subdir, skip_recurse, &sub_xattrs_count))
      *url_xattrs_count += sub_xattrs_count;
    else
      ret = false;
  }

  return ret;
}

}  // namespace file_attrs_cleaner
