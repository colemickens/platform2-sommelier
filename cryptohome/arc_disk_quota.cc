// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/arc_disk_quota.h"

#include <base/files/file_enumerator.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>

namespace cryptohome {

ArcDiskQuota::ArcDiskQuota(Platform* platform,
                           const base::FilePath& home,
                           const base::FilePath& shadow_root_relative_path,
                           const base::FilePath& android_data_relative_path)
    : platform_(platform),
      home_(home),
      shadow_root_relative_path_(shadow_root_relative_path),
      android_data_relative_path_(android_data_relative_path) {}

ArcDiskQuota::~ArcDiskQuota() = default;

void ArcDiskQuota::Initialize() {
  device_ = GetDevice();
}

bool ArcDiskQuota::IsQuotaSupported() const {
  if (device_.empty()) {
    LOG(ERROR) << "No quota mount is found.";
    return false;
  }

  // TODO(risan): Support quota for more than 1 Android user,
  // after that, the following check could be removed.
  int cnt = GetAndroidUserCount();
  if (cnt != 1) {
    LOG(ERROR) << "Quota is supported only for 1 Android user, found " << cnt
               << " Android users.";
    return false;
  }

  return true;
}

int64_t ArcDiskQuota::GetCurrentSpaceForUid(uid_t uid) const {
  if (device_.empty()) {
    LOG(ERROR) << "No quota mount is found";
    return -1;
  }
  int64_t current_space = platform_->GetQuotaCurrentSpaceForUid(device_, uid);
  if (current_space == -1) {
    PLOG(ERROR) << "Failed to get disk stats for uid: " << uid;
    return -1;
  }
  return current_space;
}

int64_t ArcDiskQuota::GetCurrentSpaceForGid(gid_t gid) const {
  if (device_.empty()) {
    LOG(ERROR) << "No quota mount is found";
    return -1;
  }
  int64_t current_space = platform_->GetQuotaCurrentSpaceForGid(device_, gid);
  if (current_space == -1) {
    PLOG(ERROR) << "Failed to get disk stats for gid: " << gid;
    return -1;
  }
  return current_space;
}

base::FilePath ArcDiskQuota::GetDevice() {
  std::string device;
  if (!platform_->FindFilesystemDevice(home_, &device)) {
    LOG(ERROR) << "Home device is not found.";
    return base::FilePath();
  }

  // Check if the device is mounted with quota option.
  if (platform_->GetQuotaCurrentSpaceForUid(base::FilePath(device), 0) < 0) {
    LOG(ERROR) << "Device is not mounted with quota feature enabled.";
    return base::FilePath();
  }

  return base::FilePath(device);
}

int32_t ArcDiskQuota::GetAndroidUserCount() const {
  // Get the number of users that contains android-data under:
  // /home/.shadow/<HASH>/mount/root/android-data
  FileEnumerator* dir_enum =
      platform_->GetFileEnumerator(home_.Append(shadow_root_relative_path_),
                                   false, base::FileEnumerator::DIRECTORIES);

  // The number of user directories that contains android-data dir.
  int number_of_android_users = 0;
  for (base::FilePath user_dir = dir_enum->Next(); !user_dir.empty();
       user_dir = dir_enum->Next()) {
    base::FilePath data_dir = user_dir.Append(android_data_relative_path_);
    if (platform_->FileExists(data_dir)) {
      ++number_of_android_users;
    }
  }
  return number_of_android_users;
}

}  // namespace cryptohome
