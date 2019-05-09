// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/drivefs_helper.h"

#include <base/files/file_path.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>

#include "cros-disks/fuse_mounter.h"
#include "cros-disks/mount_options.h"
#include "cros-disks/platform.h"
#include "cros-disks/system_mounter.h"
#include "cros-disks/uri.h"

namespace cros_disks {
namespace {

const char kDataDirOptionPrefix[] = "datadir=";
const char kIdentityOptionPrefix[] = "identity=";

const char kUserName[] = "fuse-drivefs";
const char kHelperTool[] = "/opt/google/drive-file-stream/drivefs";
const char kSeccompPolicyFile[] =
    "/opt/google/drive-file-stream/drivefs-seccomp.policy";
const char kType[] = "drivefs";
const char kDbusSocketPath[] = "/run/dbus";

}  // namespace

DrivefsHelper::DrivefsHelper(const Platform* platform)
    : FUSEHelper(kType, platform, base::FilePath(kHelperTool), kUserName) {}

DrivefsHelper::~DrivefsHelper() = default;

std::unique_ptr<FUSEMounter> DrivefsHelper::CreateMounter(
    const base::FilePath& working_dir,
    const Uri& source,
    const base::FilePath& target_path,
    const std::vector<std::string>& options) const {
  const std::string& identity = source.path();

  // Enforced by FUSEHelper::CanMount().
  DCHECK(!identity.empty());

  auto data_dir = GetValidatedDataDir(options);
  if (data_dir.empty()) {
    return nullptr;
  }

  uid_t files_uid;
  gid_t files_gid;
  if (!platform()->GetUserAndGroupId(kFilesUser, &files_uid, nullptr) ||
      !platform()->GetGroupId(kFilesGroup, &files_gid)) {
    LOG(ERROR) << "Invalid user configuration.";
    return nullptr;
  }
  if (!SetupDirectoryForFUSEAccess(data_dir)) {
    return nullptr;
  }
  MountOptions mount_options;
  mount_options.EnforceOption(kDataDirOptionPrefix + data_dir.value());
  mount_options.EnforceOption(kIdentityOptionPrefix + identity);
  mount_options.Initialize(options, true, base::IntToString(files_uid),
                           base::IntToString(files_gid));

  // TODO(crbug.com/859802): Make seccomp mandatory when testing done.
  std::string seccomp =
      platform()->PathExists(kSeccompPolicyFile) ? kSeccompPolicyFile : "";

  // Bind datadir and DBus communication socket into the sandbox.
  std::vector<FUSEMounter::BindPath> paths = {
      {data_dir.value(), true /* writable */},
      {kDbusSocketPath, true},
  };
  return std::make_unique<FUSEMounter>(
      "", target_path.value(), type(), mount_options, platform(),
      program_path().value(), user(), seccomp, paths, true, true);
}

base::FilePath DrivefsHelper::GetValidatedDataDir(
    const std::vector<std::string>& options) const {
  for (const auto& option : options) {
    if (base::StartsWith(option, kDataDirOptionPrefix,
                         base::CompareCase::SENSITIVE)) {
      std::string path_string = option.substr(strlen(kDataDirOptionPrefix));
      base::FilePath data_dir(path_string);
      if (data_dir.empty() || !data_dir.IsAbsolute() ||
          data_dir.ReferencesParent()) {
        LOG(ERROR) << "Invalid DriveFS option datadir=" << path_string;
        return {};
      }
      base::FilePath suffix_component;
      // If the datadir doesn't exist, canonicalize the parent directory
      // instead, and append the last path component to that path.
      if (!platform()->DirectoryExists(data_dir.value())) {
        suffix_component = data_dir.BaseName();
        data_dir = data_dir.DirName();
      }
      if (!platform()->GetRealPath(data_dir.value(), &path_string)) {
        return {};
      }
      return base::FilePath(path_string).Append(suffix_component);
    }
  }
  return {};
}

bool DrivefsHelper::SetupDirectoryForFUSEAccess(
    const base::FilePath& dir) const {
  CHECK(dir.IsAbsolute() && !dir.ReferencesParent())
      << "unsafe path '" << dir.value() << "'";

  uid_t files_uid, mounter_uid;
  gid_t files_gid;
  if (!platform()->GetUserAndGroupId(user(), &mounter_uid, nullptr) ||
      !platform()->GetUserAndGroupId(kFilesUser, &files_uid, nullptr) ||
      !platform()->GetGroupId(kFilesGroup, &files_gid)) {
    LOG(ERROR) << "Invalid user configuration.";
    return false;
  }
  std::string path = dir.value();

  if (platform()->DirectoryExists(path)) {
    uid_t current_uid;
    gid_t current_gid;
    if (!platform()->GetOwnership(path, &current_uid, &current_gid)) {
      LOG(WARNING) << "Can't access datadir '" << path << "'";
      return false;
    }
    if (current_uid == mounter_uid && current_gid == files_gid) {
      return true;
    }
    if (!platform()->RemoveEmptyDirectory(path)) {
      LOG(WARNING) << "Existing datadir '" << path << "' has unexpected owner "
                   << current_uid << ":" << current_gid;
      return false;
    }
  }
  if (!platform()->CreateDirectory(path)) {
    LOG(ERROR) << "Failed to create datadir '" << path << "'";
    return false;
  }
  if (!platform()->SetPermissions(path, 0770)) {
    LOG(ERROR) << "Can't chmod datadir '" << path << "'";
    return false;
  }
  if (!platform()->SetOwnership(path, mounter_uid, files_gid)) {
    LOG(ERROR) << "Can't chown datadir '" << path << "'";
    return false;
  }
  return true;
}

}  // namespace cros_disks
