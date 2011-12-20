// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/archive-manager.h"

#include <linux/capability.h>

#include <base/file_path.h>
#include <base/file_util.h>
#include <base/logging.h>
#include <base/stringprintf.h>
#include <base/string_util.h>

#include "cros-disks/metrics.h"
#include "cros-disks/mount-info.h"
#include "cros-disks/mount-options.h"
#include "cros-disks/platform.h"
#include "cros-disks/sandboxed-process.h"
#include "cros-disks/system-mounter.h"

using std::string;
using std::vector;

namespace {

// Mapping from a base path to its corresponding path inside the AVFS mount.
struct AVFSPathMapping {
  const char* const base_path;
  const char* const avfs_path;
};

// Process capabilities required by the avfsd process:
//   CAP_SYS_ADMIN for mounting/unmounting filesystem
const uint64_t kAVFSMountProgramCapabilities = 1 << CAP_SYS_ADMIN;

// Number of components in a mount directory path. A mount directory is always
// created under /media/<sub type>/<mount dir>, so it always has 4 components
// in the path: '/', 'media', '<sub type>', '<mount dir>'
size_t kNumComponentsInMountDirectoryPath = 4;
const char kAVFSMountGroup[] = "chronos-access";
const char kAVFSMountUser[] = "avfs";
// TODO(wad,benchan): Revisit the location of policy files once more system
// daemons are sandboxed with seccomp filters.
const char kAVFSSeccompFilterPolicyFile[] =
    "/opt/google/cros-disks/avfsd-seccomp.policy";
const char kAVFSMountProgram[] = "/usr/bin/avfsd";
const char kAVFSRootDirectory[] = "/var/run/avfsroot";
const char kAVFSLogFile[] = "/var/run/avfsroot/avfs.log";
const char kAVFSMediaDirectory[] = "/var/run/avfsroot/media";
const char kAVFSUserFileDirectory[] = "/var/run/avfsroot/user";
const char kMediaDirectory[] = "/media";
const char kUserFileDirectory[] = "/home/chronos/user/Downloads";
const AVFSPathMapping kAVFSPathMapping[] = {
  { kMediaDirectory, kAVFSMediaDirectory },
  { kUserFileDirectory, kAVFSUserFileDirectory },
};

}  // namespace

namespace cros_disks {

ArchiveManager::ArchiveManager(const string& mount_root,
                               Platform* platform,
                               Metrics* metrics)
    : MountManager(mount_root, platform, metrics),
      avfs_started_(false) {
}

ArchiveManager::~ArchiveManager() {
  // StopAVFS() unmounts all mounted archives as well as AVFS mount points.
  StopAVFS();
}

bool ArchiveManager::Initialize() {
  RegisterDefaultFileExtensions();
  return MountManager::Initialize();
}

bool ArchiveManager::StopSession(const string& user) {
  return StopAVFS();
}

bool ArchiveManager::CanMount(const string& source_path) const {
  // The following paths can be mounted:
  //     /home/chronos/user/Downloads/...<file>
  //     /media/<dir>/<dir>/...<file>
  FilePath file_path(source_path);
  if (FilePath(kUserFileDirectory).IsParent(file_path))
    return true;

  if (FilePath(kMediaDirectory).IsParent(file_path)) {
    vector<FilePath::StringType> components;
    file_path.StripTrailingSeparators().GetComponents(&components);
    // e.g. components = { '/', 'media', 'removable', 'usb', 'doc.zip' }
    if (components.size() > kNumComponentsInMountDirectoryPath)
      return true;
  }
  return false;
}

MountErrorType ArchiveManager::DoMount(const string& source_path,
                                       const string& filesystem_type,
                                       const vector<string>& options,
                                       const string& mount_path) {
  CHECK(!source_path.empty()) << "Invalid source path argument";
  CHECK(!mount_path.empty()) << "Invalid mount path argument";

  string extension = GetFileExtension(source_path);
  metrics()->RecordArchiveType(extension);

  string avfs_path;
  if (IsFileExtensionSupported(extension)) {
    avfs_path = GetAVFSPath(source_path);
  }
  if (avfs_path.empty()) {
    LOG(ERROR) << "Path '" << source_path << "' is not a supported archive";
    return MOUNT_ERROR_UNSUPPORTED_ARCHIVE;
  }

  if (!StartAVFS()) {
    LOG(ERROR) << "Failed to start AVFS mounts.";
    return MOUNT_ERROR_INTERNAL;
  }

  // Perform a bind mount from the archive path under the AVFS mount
  // to /media/archive/<archive name>.
  vector<string> extended_options = options;
  extended_options.push_back(MountOptions::kOptionBind);
  MountOptions mount_options;
  mount_options.Initialize(extended_options, false, "", "");
  SystemMounter mounter(avfs_path, mount_path, "", mount_options);
  return mounter.Mount();
}

MountErrorType ArchiveManager::DoUnmount(const string& path,
                                         const vector<string>& options) {
  CHECK(!path.empty()) << "Invalid path argument";
  // TODO(benchan): Extract error from low-level unmount operation.
  return platform()->Unmount(path) ? MOUNT_ERROR_NONE : MOUNT_ERROR_UNKNOWN;
}

string ArchiveManager::SuggestMountPath(const string& source_path) const {
  // Use the archive name to name the mount directory.
  FilePath base_name = FilePath(source_path).BaseName();
  return FilePath(mount_root()).Append(base_name).value();
}

bool ArchiveManager::IsFileExtensionSupported(
    const string& extension) const {
  return extensions_.find(extension) != extensions_.end();
}

void ArchiveManager::RegisterDefaultFileExtensions() {
  // TODO(benchan): Perhaps these settings can be read from a config file.
  RegisterFileExtension("zip");
}

void ArchiveManager::RegisterFileExtension(const string& extension) {
  extensions_.insert(extension);
}

string ArchiveManager::GetFileExtension(const string& path) const {
  FilePath file_path(path);
  string extension = file_path.Extension();
  if (!extension.empty()) {
    // Strip the leading dot and convert the extension to lower case.
    extension.erase(0, 1);
    StringToLowerASCII(&extension);
  }
  return extension;
}

string ArchiveManager::GetAVFSPath(const string& path) const {
  FilePath file_path(path);
  for (size_t i = 0; i < arraysize(kAVFSPathMapping); ++i) {
    FilePath base_path(kAVFSPathMapping[i].base_path);
    FilePath avfs_path(kAVFSPathMapping[i].avfs_path);
    if (base_path.AppendRelativePath(file_path, &avfs_path)) {
      return avfs_path.value() + "#";
    }
  }
  return string();
}

bool ArchiveManager::StartAVFS() {
  if (avfs_started_)
    return true;

  uid_t user_id;
  gid_t group_id;
  if (!platform()->GetUserAndGroupId(kAVFSMountUser, &user_id, &group_id) ||
      !platform()->CreateDirectory(kAVFSRootDirectory) ||
      !platform()->SetOwnership(kAVFSRootDirectory, user_id, group_id) ||
      !platform()->SetPermissions(kAVFSRootDirectory, S_IRWXU)) {
    platform()->RemoveEmptyDirectory(kAVFSRootDirectory);
    return false;
  }

  // Set the AVFS_LOGFILE environment variable so that the AVFS daemon
  // writes log messages to a file instead of syslog. Otherwise, writing
  // to syslog may trigger the socket/connect/send system calls, which are
  // disabled by the seccomp filters policy file. This only affects the
  // child processes spawned by cros-disks and does not persist after
  // cros-disks restarts.
  setenv("AVFS_LOGFILE", kAVFSLogFile, 1);

  avfs_started_ = true;
  for (size_t i = 0; i < arraysize(kAVFSPathMapping); ++i) {
    const string& avfs_path = kAVFSPathMapping[i].avfs_path;
    if (!platform()->CreateDirectory(avfs_path) ||
        !platform()->SetOwnership(avfs_path, user_id, group_id) ||
        !platform()->SetPermissions(avfs_path, S_IRWXU) ||
        !MountAVFSPath(kAVFSPathMapping[i].base_path, avfs_path)) {
      StopAVFS();
      return false;
    }
  }
  return true;
}

bool ArchiveManager::StopAVFS() {
  if (!avfs_started_)
    return true;

  avfs_started_ = false;
  // Unmounts all mounted archives before unmounting AVFS mounts.
  bool all_unmounted = UnmountAll();
  for (size_t i = 0; i < arraysize(kAVFSPathMapping); ++i) {
    const string& path = kAVFSPathMapping[i].avfs_path;
    if (!platform()->Unmount(path))
      all_unmounted = false;
    platform()->RemoveEmptyDirectory(path);
  }
  platform()->RemoveEmptyDirectory(kAVFSRootDirectory);
  return all_unmounted;
}

bool ArchiveManager::MountAVFSPath(const string& base_path,
                                   const string& avfs_path) const {
  MountInfo mount_info;
  if (!mount_info.RetrieveFromCurrentProcess())
    return false;

  if (mount_info.HasMountPath(avfs_path)) {
    LOG(WARNING) << "Path '" << avfs_path << "' is already mounted.";
    return false;
  }

  uid_t user_id;
  gid_t group_id;
  if (!platform()->GetUserAndGroupId(kAVFSMountUser, &user_id, NULL) ||
      !platform()->GetGroupId(kAVFSMountGroup, &group_id)) {
    return false;
  }

  SandboxedProcess mount_process;
  mount_process.AddArgument(kAVFSMountProgram);
  mount_process.AddArgument("-o");
  mount_process.AddArgument(base::StringPrintf(
      "ro,nodev,noexec,nosuid,allow_other,user=%s,modules=subdir,subdir=%s",
      kAVFSMountUser, base_path.c_str()));
  mount_process.AddArgument(avfs_path);
  if (file_util::PathExists(FilePath(kAVFSSeccompFilterPolicyFile))) {
    mount_process.LoadSeccompFilterPolicy(kAVFSSeccompFilterPolicyFile);
  } else {
    // TODO(benchan): Remove this fallback mechanism once we have policy files
    //                for all supported platforms.
    LOG(WARNING) << "Seccomp filter policy '" << kAVFSSeccompFilterPolicyFile
                 << "' not found. Use POSIX capabilities mechanism instead";
    mount_process.SetCapabilities(kAVFSMountProgramCapabilities);
  }
  // TODO(benchan): Enable PID and VFS namespace.
  // TODO(wad,ellyjones,benchan): Enable network namespace once libminijail
  // supports it.
  mount_process.SetUserId(user_id);
  mount_process.SetGroupId(group_id);
  if (mount_process.Run() != 0 ||
      !mount_info.RetrieveFromCurrentProcess() ||
      !mount_info.HasMountPath(avfs_path)) {
    LOG(WARNING) << "Failed to mount '" << base_path << "' to '"
                 << avfs_path << "' via AVFS";
    return false;
  }

  LOG(INFO) << "Mounted '" << base_path << "' to '" << avfs_path
            << "' via AVFS";
  return true;
}

}  // namespace cros_disks
