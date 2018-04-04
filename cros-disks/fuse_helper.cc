// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/fuse_helper.h"

#include <memory>

#include <base/files/file_path.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>

#include "cros-disks/fuse_mounter.h"
#include "cros-disks/mount_options.h"
#include "cros-disks/platform.h"

using std::string;

namespace cros_disks {

const char FUSEHelper::kFilesUser[] = "chronos";
const char FUSEHelper::kFilesGroup[] = "chronos-access";
const char FUSEHelper::kOptionAllowOther[] = "allow_other";

FUSEHelper::FUSEHelper(const std::string& fuse_type,
                       const Platform* platform,
                       const std::string& mount_program_path,
                       const std::string& mount_user)
    : fuse_type_(fuse_type),
      platform_(platform),
      mount_program_path_(mount_program_path),
      mount_user_(mount_user) {}

FUSEHelper::~FUSEHelper() = default;

MountErrorType FUSEHelper::Mount(
    const std::string& source,
    const std::string& target_path,
    const std::vector<std::string>& options) const {
  uid_t uid;
  gid_t gid;
  if (!platform_->GetUserAndGroupId(kFilesUser, &uid, &gid) ||
      !platform_->GetGroupId(kFilesGroup, &gid)) {
    LOG(ERROR) << "Invalid user configuration for '" << fuse_type_ << "'";
    return MOUNT_ERROR_INTERNAL;
  }

  MountOptions mount_options;
  if (!PrepareMountOptions(options, uid, gid, &mount_options)) {
    LOG(ERROR) << "Invalid options for source '" << source << "'";
    return MOUNT_ERROR_INVALID_MOUNT_OPTIONS;
  }

  auto mounter = std::make_unique<FUSEMounter>(
      source, target_path, fuse_type_, mount_options, platform_,
      mount_program_path_, mount_user_);
  if (!mounter) {
    LOG(ERROR) << "Failed to create mounter for type '" << fuse_type_
               << "' and source '" << source << "'";
    return MOUNT_ERROR_INVALID_MOUNT_OPTIONS;
  }
  return mounter->Mount();
}

bool FUSEHelper::PrepareMountOptions(const std::vector<std::string>& options,
                                     uid_t uid,
                                     gid_t gid,
                                     MountOptions* mount_options) const {
  mount_options->Initialize(options, false, base::StringPrintf("%d", uid),
                            base::StringPrintf("%d", gid));
  return true;
}

bool FUSEHelper::SetupDirectoryForFUSEAccess(const std::string& dirpath) const {
  base::FilePath dir(dirpath);
  CHECK(dir.IsAbsolute() && !dir.ReferencesParent())
      << "unsafe path '" << dir.value() << "'";

  uid_t files_uid, mounter_uid;
  gid_t files_gid;
  if (!platform_->GetUserAndGroupId(mount_user_, &mounter_uid, nullptr) ||
      !platform_->GetUserAndGroupId(kFilesUser, &files_uid, nullptr) ||
      !platform_->GetGroupId(kFilesGroup, &files_gid)) {
    LOG(ERROR) << "Invalid user configuration.";
    return false;
  }
  std::string path = dir.value();

  if (!platform_->DirectoryExists(path)) {
    if (!platform_->CreateDirectory(path)) {
      LOG(WARNING) << "Failed to create datadir '" << path << "'";
      return false;
    } else if (!platform_->SetPermissions(path, 0770)) {
      LOG(WARNING) << "Can't chmod datadir.";
      // This is not critical, let's continue.
    }
  } else {
    uid_t current_uid;
    gid_t current_gid;
    if (!platform_->GetOwnership(path, &current_uid, &current_gid)) {
      LOG(WARNING) << "Can't access datadir '" << path << "'";
      return false;
    }
    if (current_uid == mounter_uid && current_gid == files_gid) {
      // Nothing to do. Dir is already owned by the right user.
      return true;
    }
    if (current_uid != files_uid) {
      LOG(WARNING) << "Not owned by chronos '" << path << "'";
      return false;
    }
  }

  if (!platform_->IsDirectoryEmpty(path)) {
    LOG(WARNING) << "Skipping non-empty directory '" << path << "'";
    return false;
  }
  if (!platform_->SetOwnership(path, mounter_uid, files_gid)) {
    LOG(ERROR) << "Can't chown datadir.";
    return false;
  }

  return true;
}

}  // namespace cros_disks
