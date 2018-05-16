// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DiskQuota - manages the kernel's quota-related operations.

#ifndef CRYPTOHOME_ARC_DISK_QUOTA_H_
#define CRYPTOHOME_ARC_DISK_QUOTA_H_

#include <cstdint>
#include <memory>
#include <string>

#include <base/macros.h>

#include "cryptohome/platform.h"

namespace cryptohome {

// This class handles quota-related query from ARC++, and only designed to be
// called from within the container. The main reason is that IsQuotaSupported
// only makes sense from within the container since it counts the number of
// mounted android-data and only makes sense when the current user's
// android-data is mounted (which depends strictly on the container startup
// sequence: android-data is explicitly mounted before this function is called
// in installd, which is defined in init).
//
// This class only caches the device file that contains the home directory,
// since the device file won't change throughout Cryptohome lifetime. On the
// other hand, IsQuotaSupported is not cached here (please see the comments in
// IsQuotaSupported for the more detailed explanation).
class ArcDiskQuota {
 public:
  // Parameters
  //   platform - The mockable Cryptohome platform
  //   home - The path to the home directory, e.g., /home
  //   shadow_root_relative_path - The relative path to the shadow root, e.g.,
  //       .shadow
  //   android_data_relative_path - The relative path to the android-data, e.g.,
  //       mount/root/android-data
  ArcDiskQuota(Platform* platform,
               const base::FilePath& home,
               const base::FilePath& shadow_root_relative_path,
               const base::FilePath& android_data_relative_path);

  virtual ~ArcDiskQuota();

  // Initializing by looking for the right quota mounted device that hosts
  // Android's /data. Not thread-safe.
  virtual void Initialize();

  // Whether or not cryptohome supports quota-based stats. This function returns
  // true when all the following conditions are true:
  // 1. There is a /dev file mounted as /home
  // 2. The dev file above is mounted with quota option enabled
  // 3. There is exactly 1 android-data mounted.
  //
  // Before multiple Android user is supported, make sure to call this
  // function (once) from Android container (i.e., during installd
  // initialization) before asking for curspace. Moreover, this function
  // shouldn't be called too often since it iterates through filesystem and
  // might potentially be expensive.
  //
  // Caching note: This function is intentionally not cached in cryptohome, but
  // should be cached in installd instead, since cryptohome lifetime is
  // different from container's (and the android-data directory). However,
  // caching this during installd's initialization might produce false negative
  // during installd's lifetime. For example in the case when cryptohome
  // concurrently cleans up old users due to low storage event - which might
  // reduce the number of android-data from more than 1 to 1. However, this case
  // should be rare and even if that happens, installd still works correctly
  // using non-quota path.
  // On the other hand, false positive is not desired (since triggering quota
  // path on multiple user will gave undesired result). Fortunately, caching
  // this function in installd won't result to false positive because installd
  // is restarted after everytime android-data is mounted as /data - and hence,
  // there won't be a case where new android-data is mounted in the middle of
  // installd lifetime.
  virtual bool IsQuotaSupported() const;

  // Get the current disk space usage for a uid. Returns -1 if quotactl fails.
  virtual int64_t GetCurrentSpaceForUid(uid_t uid) const;

  // Get the current disk space usage for a gid. Returns -1 if quotactl fails.
  virtual int64_t GetCurrentSpaceForGid(gid_t gid) const;

 private:
  // Helper function to parse dev file that contains Android's /data.
  base::FilePath GetDevice();

  // Helper function to get the number of users with Android's /data.
  virtual int32_t GetAndroidUserCount() const;

  Platform* platform_;
  const base::FilePath home_;
  const base::FilePath shadow_root_relative_path_;
  const base::FilePath android_data_relative_path_;
  base::FilePath device_;

  friend class ArcDiskQuotaTest;

  DISALLOW_COPY_AND_ASSIGN(ArcDiskQuota);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_ARC_DISK_QUOTA_H_
