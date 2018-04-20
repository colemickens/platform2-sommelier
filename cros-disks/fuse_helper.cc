// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/fuse_helper.h"

#include <algorithm>
#include <memory>

#include <base/logging.h>

#include "cros-disks/fuse_mounter.h"
#include "cros-disks/mount_options.h"
#include "cros-disks/platform.h"
#include "cros-disks/system_mounter.h"
#include "cros-disks/uri.h"

namespace cros_disks {

const char FUSEHelper::kFilesUser[] = "chronos";
const char FUSEHelper::kFilesGroup[] = "chronos-access";
const char FUSEHelper::kOptionAllowOther[] = "allow_other";
const char FUSEHelper::kOptionDefaultPermissions[] = "default_permissions";

FUSEHelper::FUSEHelper(const std::string& fuse_type,
                       const Platform* platform,
                       const base::FilePath& mount_program_path,
                       const std::string& mount_user)
    : fuse_type_(fuse_type),
      platform_(platform),
      mount_program_path_(mount_program_path),
      mount_user_(mount_user) {}

FUSEHelper::~FUSEHelper() = default;

bool FUSEHelper::CanMount(const Uri& source) const {
  return source.scheme() == type() && !source.path().empty();
}

std::string FUSEHelper::GetTargetSuffix(const Uri& source) const {
  std::string path = source.path();
  std::replace(path.begin(), path.end(), '/', '$');
  std::replace(path.begin(), path.end(), '.', '_');
  return path;
}

std::unique_ptr<FUSEMounter> FUSEHelper::CreateMounter(
    const base::FilePath& working_dir,
    const Uri& source,
    const base::FilePath& target_path,
    const std::vector<std::string>& options) const {
  MountOptions mount_options;
  mount_options.Initialize(options, false, "", "");
  return std::make_unique<FUSEMounter>(source.path(), target_path.value(),
                                       type(), mount_options, platform(),
                                       program_path().value(), user(), false);
}

bool FUSEHelper::SetupDirectoryForFUSEAccess(const base::FilePath& dir) const {
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
