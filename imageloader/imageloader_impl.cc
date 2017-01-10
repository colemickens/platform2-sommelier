// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "imageloader_impl.h"

#include <sys/statvfs.h>
#include <sys/vfs.h>
#include </usr/include/linux/magic.h>

#include <string>

#include <base/containers/adapters.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/important_file_writer.h>
#include <base/logging.h>
#include <base/version.h>
#include <chromeos/dbus/service_constants.h>

#include "component.h"

namespace imageloader {

namespace {

using imageloader::kBadResult;

// The name of the file containing the latest component version.
constexpr char kLatestVersionFile[] = "latest-version";
// The maximum size of the latest-version file.
constexpr int kMaximumLatestVersionSize = 4096;

// |mount_base_path| is the subfolder where all components are mounted.
// For example "/mnt/imageloader."
base::FilePath GetMountPoint(const base::FilePath& mount_base_path,
                             const std::string& component_name,
                             const std::string& component_version) {
  return mount_base_path.Append(component_name).Append(component_version);
}

bool AssertComponentDirPerms(const base::FilePath& path) {
  int mode;
  if (!GetPosixFilePermissions(path, &mode)) return false;
  return mode == kComponentDirPerms;
}

bool CreateDirectoryWithMode(const base::FilePath& full_path, int mode) {
  std::vector<base::FilePath> subpaths;

  // Collect a list of all parent directories.
  base::FilePath last_path = full_path;
  subpaths.push_back(full_path);
  for (base::FilePath path = full_path.DirName();
       path.value() != last_path.value(); path = path.DirName()) {
    subpaths.push_back(path);
    last_path = path;
  }

  // Iterate through the parents and create the missing ones.
  for (const auto& subpath : base::Reversed(subpaths)) {
    if (base::DirectoryExists(subpath)) continue;
    if (mkdir(subpath.value().c_str(), mode) == 0) continue;
    // Mkdir failed, but it might have failed with EEXIST, or some other error
    // due to the the directory appearing out of thin air. This can occur if
    // two processes are trying to create the same file system tree at the same
    // time. Check to see if it exists and make sure it is a directory.
    if (!base::DirectoryExists(subpath)) {
      PLOG(ERROR) << "Failed to create directory";
      return false;
    }
  }
  return true;
}

bool CreateMountPointIfNeeded(const base::FilePath& mount_point,
                              bool* already_mounted) {
  *already_mounted = false;
  // Is this mount point somehow already taken?
  struct stat st;
  if (lstat(mount_point.value().c_str(), &st) == 0) {
    if (!S_ISDIR(st.st_mode)) {
      LOG(ERROR) << "Mount point exists but is not a directory.";
      return false;
    }

    base::FilePath mount_parent = mount_point.DirName();
    struct stat st2;
    if (stat(mount_parent.value().c_str(), &st2) != 0) {
      PLOG(ERROR) << "Could not stat the mount point parent";
      return false;
    }
    if (st.st_dev != st2.st_dev) {
      struct statfs st_fs;
      if (statfs(mount_point.value().c_str(), &st_fs) != 0) {
        PLOG(ERROR) << "statfs";
        return false;
      }
      if (st_fs.f_type != SQUASHFS_MAGIC || !(st_fs.f_flags & ST_NODEV) ||
          !(st_fs.f_flags & ST_NOSUID) || !(st_fs.f_flags & ST_RDONLY)) {
        LOG(ERROR) << "File system is not the expected type.";
        return false;
      }
      LOG(INFO) << "The mount point already exists: " << mount_point.value();
      *already_mounted = true;
      return true;
    }
  } else if (!CreateDirectoryWithMode(mount_point, kComponentDirPerms)) {
    LOG(ERROR) << "Failed to create mount point: " << mount_point.value();
    return false;
  }
  return true;
}

}  // namespace {}

bool ImageLoaderImpl::LoadComponent(const std::string& name,
                                    const std::string& mount_point_str) {
  base::FilePath component_path;
  if (!GetPathToCurrentComponentVersion(name, &component_path)) {
    return false;
  }

  Component component(component_path);
  if (!component.Init(config_.key)) {
    LOG(ERROR) << "Failed to initialize component: " << name;
    return false;
  }

  base::FilePath mount_point(mount_point_str);
  // First check if the component is already mounted and avoid unnecessary work.
  bool already_mounted = false;
  if (!CreateMountPointIfNeeded(mount_point, &already_mounted)) return false;
  if (already_mounted) return true;

  return component.Mount(config_.verity_mounter.get(), mount_point);
}

std::string ImageLoaderImpl::LoadComponent(const std::string& name) {
  base::FilePath component_path;
  if (!GetPathToCurrentComponentVersion(name, &component_path)) {
    return kBadResult;
  }

  Component component(component_path);
  if (!component.Init(config_.key)) {
    LOG(ERROR) << "Failed to initialize component: " << name;
    return kBadResult;
  }

  base::FilePath mount_point(
      GetMountPoint(config_.mount_path, name, component.manifest().version));
  // First check if the component is already mounted and avoid unnecessary work.
  bool already_mounted = false;
  if (!CreateMountPointIfNeeded(mount_point, &already_mounted))
    return kBadResult;
  if (already_mounted) return name;

  return component.Mount(config_.verity_mounter.get(), mount_point)
             ? mount_point.value()
             : kBadResult;
}

bool ImageLoaderImpl::RegisterComponent(
    const std::string& name, const std::string& version,
    const std::string& component_folder_abs_path) {
  base::FilePath components_dir(config_.storage_dir);

  // If the directory is writable by others, do not trust the components.
  if (!AssertComponentDirPerms(components_dir)) return false;

  std::string old_version_hint;
  base::FilePath version_hint_path(GetLatestVersionFilePath(name));
  bool have_old_version = base::PathExists(version_hint_path);
  if (have_old_version) {
    if (!base::ReadFileToStringWithMaxSize(version_hint_path, &old_version_hint,
                                           kMaximumLatestVersionSize)) {
      return false;
    }

    // Check for version rollback.
    base::Version current_version(old_version_hint);
    base::Version new_version(version);
    if (!new_version.IsValid()) {
      return false;
    }

    if (current_version.IsValid() && new_version <= current_version) {
      LOG(ERROR) << "Version [" << new_version << "] is not newer than ["
                 << current_version << "] for component [" << name
                 << "] and cannot be registered.";
      return false;
    }
  }

  // Check if this specific component already exists in the filesystem.
  base::FilePath component_root(GetComponentRoot(name));
  if (!base::PathExists(component_root)) {
    if (mkdir(component_root.value().c_str(), kComponentDirPerms) != 0) {
      PLOG(ERROR) << "Could not create component specific directory.";
      return false;
    }
  }

  base::FilePath component_path(component_folder_abs_path);
  Component component(component_path);
  if (!component.Init(config_.key)) return false;

  // Check that the reported version matches the component manifest version.
  if (component.manifest().version != version) {
    LOG(ERROR) << "Version in signed manifest does not match the reported "
                  "component version.";
    return false;
  }

  // Take ownership of the component and verify it.
  base::FilePath version_path(GetVersionPath(name, version));
  // If |version_path| exists but was not the active version, ImageLoader
  // probably crashed previously and could not cleanup.
  if (base::PathExists(version_path)) {
    base::DeleteFile(version_path, /*recursive=*/true);
  }

  if (mkdir(version_path.value().c_str(), kComponentDirPerms) != 0) {
    PLOG(ERROR) << "Could not create directory for new component version.";
    return false;
  }

  if (!component.CopyTo(version_path)) {
    base::DeleteFile(version_path, /*recursive=*/true);
    return false;
  }

  if (!base::ImportantFileWriter::WriteFileAtomically(version_hint_path,
                                                      version)) {
    base::DeleteFile(version_path, /*recursive=*/true);
    LOG(ERROR) << "Failed to update current version hint file.";
    return false;
  }

  // Now delete the old component version, if there was one.
  if (have_old_version) {
    base::DeleteFile(GetVersionPath(name, old_version_hint),
                     /*recursive=*/true);
  }

  return true;
}

std::string ImageLoaderImpl::GetComponentVersion(const std::string& name) {
  base::FilePath component_path;
  if (!GetPathToCurrentComponentVersion(name, &component_path)) {
    return kBadResult;
  }

  Component component(component_path);
  if (!component.Init(config_.key)) return kBadResult;

  return component.manifest().version;
}

base::FilePath ImageLoaderImpl::GetLatestVersionFilePath(
    const std::string& component_name) {
  return config_.storage_dir.Append(component_name).Append(kLatestVersionFile);
}

base::FilePath ImageLoaderImpl::GetVersionPath(
    const std::string& component_name, const std::string& version) {
  return config_.storage_dir.Append(component_name).Append(version);
}

base::FilePath ImageLoaderImpl::GetComponentRoot(
    const std::string& component_name) {
  return config_.storage_dir.Append(component_name);
}

bool ImageLoaderImpl::GetPathToCurrentComponentVersion(
    const std::string& component_name, base::FilePath* result) {
  base::FilePath component_root(GetComponentRoot(component_name));
  // Read the latest version file.
  std::string latest_version;
  if (!base::ReadFileToStringWithMaxSize(
          GetLatestVersionFilePath(component_name), &latest_version,
          kMaximumLatestVersionSize)) {
    LOG(ERROR) << "Failed to read latest-version file.";
    return false;
  }

  *result = component_root.Append(latest_version);
  return true;
}

}  // namespace imageloader
