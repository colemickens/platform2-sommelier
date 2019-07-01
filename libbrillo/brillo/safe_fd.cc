// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "brillo/safe_fd.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/posix/eintr_wrapper.h>
#include <brillo/syslog_logging.h>

namespace brillo {

namespace {
SafeFD::SafeFDResult MakeErrorResult(SafeFD::Error error) {
  return std::make_pair(SafeFD(), error);
}

SafeFD::SafeFDResult MakeSuccessResult(SafeFD&& fd) {
  return std::make_pair(std::move(fd), SafeFD::Error::kNoError);
}

SafeFD::SafeFDResult OpenPathComponentInternal(int parent_fd,
                                               const std::string& file,
                                               int flags,
                                               mode_t mode) {
  if (file != "/" && file.find("/") != std::string::npos) {
    return MakeErrorResult(SafeFD::Error::kBadArgument);
  }
  SafeFD fd;

  // O_NONBLOCK is used to avoid hanging on edge cases (e.g. a serial port with
  // flow control, or a FIFO without a writer).
  if (parent_fd >= 0 || parent_fd == AT_FDCWD) {
    fd.UnsafeReset(HANDLE_EINTR(openat(parent_fd, file.c_str(),
                                       flags | O_NONBLOCK | O_NOFOLLOW, mode)));
  } else if (file == "/") {
    fd.UnsafeReset(HANDLE_EINTR(open(
        file.c_str(), flags | O_DIRECTORY | O_NONBLOCK | O_NOFOLLOW, mode)));
  }

  if (!fd.is_valid()) {
    // open(2) fails with ELOOP when the last component of the |path| is a
    // symlink. It fails with ENXIO when |path| is a FIFO and |flags| is for
    // writing because of the O_NONBLOCK flag added above.
    switch (errno) {
      case ELOOP:
        // PLOG prints something along the lines of the symlink depth being too
        // great which is is misleading so LOG is used instead.
        LOG(ERROR) << "Symlink detected! failed to open \"" << file
                   << "\" safely.";
        return MakeErrorResult(SafeFD::Error::kSymlinkDetected);
      case EISDIR:
        PLOG(ERROR) << "Directory detected! failed to open \"" << file
                    << "\" safely";
        return MakeErrorResult(SafeFD::Error::kWrongType);
      case ENOTDIR:
        PLOG(ERROR) << "Not a directory! failed to open \"" << file
                    << "\" safely";
        return MakeErrorResult(SafeFD::Error::kWrongType);
      case ENXIO:
        PLOG(ERROR) << "FIFO detected! failed to open \"" << file
                    << "\" safely";
        return MakeErrorResult(SafeFD::Error::kWrongType);
      default:
        PLOG(ERROR) << "Failed to open \"" << file << '"';
        return MakeErrorResult(SafeFD::Error::kIOError);
    }
  }

  // Remove the O_NONBLOCK flag unless the original |flags| have it.
  if ((flags & O_NONBLOCK) == 0) {
    flags = fcntl(fd.get(), F_GETFL);
    if (flags == -1) {
      PLOG(ERROR) << "Failed to get fd flags for " << file;
      return MakeErrorResult(SafeFD::Error::kIOError);
    }
    if (fcntl(fd.get(), F_SETFL, flags & ~O_NONBLOCK)) {
      PLOG(ERROR) << "Failed to set fd flags for " << file;
      return MakeErrorResult(SafeFD::Error::kIOError);
    }
  }

  return MakeSuccessResult(std::move(fd));
}

SafeFD::SafeFDResult OpenSafelyInternal(int parent_fd,
                                        const base::FilePath& path,
                                        int flags,
                                        mode_t mode) {
  std::vector<std::string> components;
  path.GetComponents(&components);

  auto itr = components.begin();
  if (itr == components.end()) {
    LOG(ERROR) << "A path is required.";
    return MakeErrorResult(SafeFD::Error::kBadArgument);
  }

  SafeFD::SafeFDResult child_fd;
  int parent_flags = flags | O_NONBLOCK | O_RDONLY | O_DIRECTORY | O_PATH;
  for (; itr + 1 != components.end(); ++itr) {
    child_fd = OpenPathComponentInternal(parent_fd, *itr, parent_flags, 0);
    // Operation failed, so directly return the error result.
    if (!child_fd.first.is_valid()) {
      return child_fd;
    }
    parent_fd = child_fd.first.get();
  }

  return OpenPathComponentInternal(parent_fd, *itr, flags, mode);
}

SafeFD::Error CheckAttributes(int fd,
                              mode_t permissions,
                              uid_t uid,
                              gid_t gid) {
  struct stat fd_attributes;
  if (fstat(fd, &fd_attributes) != 0) {
    PLOG(ERROR) << "fstat failed";
    return SafeFD::Error::kIOError;
  }

  if (fd_attributes.st_uid != uid) {
    LOG(ERROR) << "Owner uid is " << fd_attributes.st_uid << " instead of "
               << uid;
    return SafeFD::Error::kWrongUID;
  }

  if (fd_attributes.st_gid != gid) {
    LOG(ERROR) << "Owner gid is " << fd_attributes.st_gid << " instead of "
               << gid;
    return SafeFD::Error::kWrongGID;
  }

  if ((0777 & (fd_attributes.st_mode ^ permissions)) != 0) {
    mode_t mask = umask(0);
    umask(mask);
    LOG(ERROR) << "Permissions are " << std::oct
               << (0777 & fd_attributes.st_mode) << " instead of "
               << (0777 & permissions) << ". Umask is " << std::oct << mask
               << std::dec;
    return SafeFD::Error::kWrongPermissions;
  }

  return SafeFD::Error::kNoError;
}

SafeFD::Error GetFileSize(int fd, size_t* file_size) {
  struct stat fd_attributes;
  if (fstat(fd, &fd_attributes) != 0) {
    return SafeFD::Error::kIOError;
  }

  *file_size = fd_attributes.st_size;
  return SafeFD::Error::kNoError;
}

}  // namespace

const char* SafeFD::RootPath = "/";

SafeFD::SafeFDResult SafeFD::Root() {
  SafeFD::SafeFDResult root =
      OpenPathComponentInternal(-1, "/", O_DIRECTORY, 0);
  if (strcmp(SafeFD::RootPath, "/") == 0) {
    return root;
  }

  if (!root.first.is_valid()) {
    LOG(ERROR) << "Failed to open root directory!";
    return root;
  }
  return root.first.OpenExistingDir(base::FilePath(SafeFD::RootPath));
}

void SafeFD::SetRootPathForTesting(const char* new_root_path) {
  SafeFD::RootPath = new_root_path;
}

int SafeFD::get() const {
  return fd_.get();
}

bool SafeFD::is_valid() const {
  return fd_.is_valid();
}

void SafeFD::reset() {
  return fd_.reset();
}

void SafeFD::UnsafeReset(int fd) {
  return fd_.reset(fd);
}

SafeFD::Error SafeFD::Write(const char* data, size_t size) {
  if (!fd_.is_valid()) {
    LOG(WARNING) << "Called Write() on invalid SafeFD()!";
  }
  errno = 0;
  if (!base::WriteFileDescriptor(fd_.get(), data, size)) {
    PLOG(ERROR) << "Failed to write to file";
    return SafeFD::Error::kIOError;
  }

  if (HANDLE_EINTR(ftruncate(fd_.get(), size)) != 0) {
    PLOG(ERROR) << "Failed to truncate file";
    return SafeFD::Error::kIOError;
  }
  return SafeFD::Error::kNoError;
}

std::pair<std::vector<char>, SafeFD::Error> SafeFD::ReadContents(
    size_t max_size) {
  size_t file_size = 0;
  SafeFD::Error err = GetFileSize(fd_.get(), &file_size);
  if (err != SafeFD::Error::kNoError) {
    return std::make_pair(std::vector<char>{}, err);
  }

  if (file_size > max_size) {
    return std::make_pair(std::vector<char>{}, SafeFD::Error::kExceededMaximum);
  }

  std::vector<char> buffer;
  buffer.resize(file_size);

  err = Read(buffer.data(), buffer.size());
  if (err != SafeFD::Error::kNoError) {
    return std::make_pair(std::vector<char>{}, err);
  }
  return std::make_pair(std::move(buffer), err);
}

SafeFD::Error SafeFD::Read(char* data, size_t size) {
  if (!base::ReadFromFD(fd_.get(), data, size)) {
    PLOG(ERROR) << "Failed to read file";
    return SafeFD::Error::kIOError;
  }
  return SafeFD::Error::kNoError;
}

SafeFD::SafeFDResult SafeFD::OpenExistingFile(const base::FilePath& path,
                                              int flags) {
  if (!fd_.is_valid()) {
    return MakeErrorResult(SafeFD::Error::kNotInitialized);
  }

  return OpenSafelyInternal(get(), path, flags, 0 /*mode*/);
}

SafeFD::SafeFDResult SafeFD::OpenExistingDir(const base::FilePath& path,
                                             int flags) {
  if (!fd_.is_valid()) {
    return MakeErrorResult(SafeFD::Error::kNotInitialized);
  }

  return OpenSafelyInternal(get(), path, O_DIRECTORY | flags /*flags*/,
                            0 /*mode*/);
}

SafeFD::SafeFDResult SafeFD::MakeFile(const base::FilePath& path,
                                      mode_t permissions,
                                      uid_t uid,
                                      gid_t gid,
                                      int flags) {
  if (!fd_.is_valid()) {
    return MakeErrorResult(SafeFD::Error::kNotInitialized);
  }

  // Open (and create if necessary) the parent directory.
  base::FilePath dir_name = path.DirName();
  SafeFD::SafeFDResult parent_dir;
  int parent_dir_fd = get();
  if (!dir_name.empty()) {
    // Apply execute permission where read permission are present for parent
    // directories
    int dir_permissions = permissions | ((permissions & 0444) >> 2);
    parent_dir =
        MakeDir(dir_name, dir_permissions, uid, gid, O_RDONLY | O_CLOEXEC);
    if (!parent_dir.first.is_valid()) {
      return parent_dir;
    }
    parent_dir_fd = parent_dir.first.get();
  }

  // If file already exists, validate permissions.
  SafeFDResult file = OpenPathComponentInternal(
      parent_dir_fd, path.BaseName().value(), flags, permissions /*mode*/);
  if (file.first.is_valid()) {
    SafeFD::Error err =
        CheckAttributes(file.first.get(), permissions, uid, gid);
    if (err != SafeFD::Error::kNoError) {
      return MakeErrorResult(err);
    }
    return file;
  } else if (errno != ENOENT) {
    return file;
  }

  // The file does exist, create it and set the ownership.
  file =
      OpenPathComponentInternal(parent_dir_fd, path.BaseName().value(),
                                O_CREAT | O_EXCL | flags, permissions /*mode*/);
  if (!file.first.is_valid()) {
    return file;
  }
  if (HANDLE_EINTR(fchown(file.first.get(), uid, gid)) != 0) {
    PLOG(ERROR) << "Failed to set ownership in MakeFile() for \""
                << path.value() << '"';
    return MakeErrorResult(SafeFD::Error::kIOError);
  }
  return file;
}

SafeFD::SafeFDResult SafeFD::MakeDir(const base::FilePath& path,
                                     mode_t permissions,
                                     uid_t uid,
                                     gid_t gid,
                                     int flags) {
  if (!fd_.is_valid()) {
    return MakeErrorResult(SafeFD::Error::kNotInitialized);
  }

  std::vector<std::string> components;
  path.GetComponents(&components);
  if (components.empty()) {
    LOG(ERROR) << "Called MakeDir() with an empty path";
    return MakeErrorResult(SafeFD::Error::kBadArgument);
  }

  // Walk the path creating directories as necessary.
  auto itr = components.begin();
  SafeFD parent_fd;
  int parent_flags = O_NONBLOCK | O_RDONLY | O_DIRECTORY | O_PATH;
  bool made_dir = false;
  while (itr + 1 != components.end()) {
    SafeFDResult child = OpenPathComponentInternal(parent_fd.get(), *itr,
                                                   parent_flags, 0 /*mode*/);
    if (!child.first.is_valid()) {
      return child;
    }
    parent_fd = std::move(child.first);

    ++itr;

    if (mkdirat(parent_fd.get(), itr->c_str(), permissions) != 0) {
      if (errno != EEXIST) {
        PLOG(ERROR) << "Failed to mkdirat() " << *itr << ": full_path=\""
                    << path.value() << '"';
        return MakeErrorResult(SafeFD::Error::kIOError);
      }
    } else {
      made_dir = true;
    }
  }

  // Open the resulting directory.
  SafeFDResult dir = OpenPathComponentInternal(parent_fd.get(), *itr,
                                               flags | O_DIRECTORY, 0 /*mode*/);
  if (!dir.first.is_valid()) {
    return dir;
  }

  if (made_dir) {
    // If the directory was created, set the ownership.
    if (HANDLE_EINTR(fchown(dir.first.get(), uid, gid)) != 0) {
      PLOG(ERROR) << "Failed to set ownership in MakeDir() for \""
                  << path.value() << '"';
      return MakeErrorResult(SafeFD::Error::kIOError);
    }
  } else {
    // If the directory already existed validate the permissions
    SafeFD::Error err = CheckAttributes(dir.first.get(), permissions, uid, gid);
    if (err != SafeFD::Error::kNoError) {
      return MakeErrorResult(err);
    }
  }

  return dir;
}

}  // namespace brillo
