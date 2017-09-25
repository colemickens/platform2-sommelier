// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/launcher/pooled_resource.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <sstream>
#include <utility>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/strings/stringprintf.h>
#include <base/values.h>

#include "vm_tools/launcher/constants.h"

namespace vm_tools {
namespace launcher {

namespace {

const size_t kBufferLength = 4096;

base::ScopedFD CreateAndLockFile(const base::FilePath& path) {
  struct flock lock;
  int rc;

  base::ScopedFD fd(HANDLE_EINTR(open(
      path.value().c_str(), O_CREAT | O_APPEND | O_RDWR | O_CLOEXEC, 0600)));
  if (!fd.is_valid()) {
    PLOG(ERROR) << "Could not open '" << path.value() << "'";
    return fd;
  }

  // Take an exclusive lock on the entire file.
  lock.l_type = F_WRLCK;
  lock.l_whence = SEEK_SET;
  lock.l_start = 0;
  lock.l_len = 0;

  rc = HANDLE_EINTR(fcntl(fd.get(), F_SETLKW, &lock));
  if (rc) {
    PLOG(ERROR) << "Failed to get write lock for '" << path.value() << "'";
    fd.reset();
  }

  return fd;
}

bool ReadFdToString(int fd, std::string* contents) {
  int rc;
  char buf[kBufferLength];

  if (!contents)
    return false;

  contents->clear();

  while ((rc = HANDLE_EINTR(read(fd, buf, kBufferLength))) > 0)
    contents->append(buf, rc);

  if (rc < 0) {
    PLOG(ERROR) << "Failed to read file";
    return false;
  }

  return true;
}

bool WriteStringToFd(int fd, const std::string& contents) {
  int rc;

  rc = HANDLE_EINTR(ftruncate(fd, 0));
  if (rc) {
    PLOG(ERROR) << "Failed to truncate file";
    return false;
  }

  if (!base::WriteFileDescriptor(fd, contents.c_str(), contents.length())) {
    LOG(ERROR) << "Failed to write file";
    return false;
  }

  return true;
}

}  // namespace

PooledResource::PooledResource(const base::FilePath& instance_runtime_dir,
                               bool release_on_destruction)
    : instance_runtime_dir_(instance_runtime_dir),
      release_on_destruction_(release_on_destruction) {}

bool PooledResource::Allocate() {
  // The fcntl lock will be dropped when fd goes out of scope.
  std::string file_path =
      base::StringPrintf("%s/%s", kVmRuntimeDirectory, GetName());
  base::ScopedFD fd = CreateAndLockFile(base::FilePath(file_path));
  if (!fd.is_valid())
    return false;

  std::string resource_file;
  if (!ReadFdToString(fd.get(), &resource_file))
    return false;

  if (!LoadGlobalResources(resource_file))
    return false;

  if (!AllocateResource())
    return false;

  if (!WriteStringToFd(fd.get(), PersistGlobalResources()))
    return false;

  if (!PersistInstanceResource())
    return false;

  return true;
}

bool PooledResource::LoadInstance() {
  // The fcntl lock will be dropped when fd goes out of scope.
  std::string global_path =
      base::StringPrintf("%s/%s", kVmRuntimeDirectory, GetName());
  base::ScopedFD global_fd = CreateAndLockFile(base::FilePath(global_path));
  base::FilePath instance_path = instance_runtime_dir_.Append(GetName());
  base::ScopedFD instance_fd = CreateAndLockFile(base::FilePath(instance_path));
  if (!global_fd.is_valid() || !instance_fd.is_valid())
    return false;

  std::string global_file;
  if (!ReadFdToString(global_fd.get(), &global_file))
    return false;

  if (!LoadGlobalResources(global_file))
    return false;

  std::string instance_file;
  if (!ReadFdToString(instance_fd.get(), &instance_file))
    return false;

  if (!LoadInstanceResource(instance_file))
    return false;

  return true;
}

bool PooledResource::Release() {
  // The fcntl lock will be dropped when fd goes out of scope.
  std::string file_path =
      base::StringPrintf("%s/%s", kVmRuntimeDirectory, GetName());
  base::ScopedFD fd = CreateAndLockFile(base::FilePath(file_path));
  if (!fd.is_valid())
    return false;

  std::string resource_file;
  if (!ReadFdToString(fd.get(), &resource_file))
    return false;

  if (!LoadGlobalResources(resource_file))
    return false;

  if (!ReleaseResource())
    return false;

  if (!WriteStringToFd(fd.get(), PersistGlobalResources()))
    return false;

  return true;
}

bool PooledResource::PersistInstanceResource() {
  // The fcntl lock will be dropped when fd goes out of scope.
  base::FilePath instance_path = instance_runtime_dir_.Append(GetName());
  base::ScopedFD fd = CreateAndLockFile(base::FilePath(instance_path));
  if (!fd.is_valid())
    return false;

  if (!WriteStringToFd(fd.get(), GetResourceID()))
    return false;

  return true;
}

bool PooledResource::ShouldReleaseOnDestruction() {
  return release_on_destruction_;
}

void PooledResource::SetReleaseOnDestruction(bool release_on_destruction) {
  release_on_destruction_ = release_on_destruction;
}

}  // namespace launcher
}  // namespace vm_tools
