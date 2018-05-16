// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_MOCK_ARC_DISK_QUOTA_H_
#define CRYPTOHOME_MOCK_ARC_DISK_QUOTA_H_

#include "cryptohome/arc_disk_quota.h"

#include <gmock/gmock.h>

namespace cryptohome {

class MockArcDiskQuota : public ArcDiskQuota {
 public:
  MockArcDiskQuota()
      : ArcDiskQuota(nullptr,
                     base::FilePath("/home"),
                     base::FilePath("/home/.shadow"),
                     base::FilePath("mount/root/android-data")) {}
  ~MockArcDiskQuota() override {}

  MOCK_METHOD0(Initialize, void());
  MOCK_CONST_METHOD0(IsQuotaSupported, bool());
  MOCK_CONST_METHOD1(GetCurrentSpaceForUid, int64_t(uid_t));
  MOCK_CONST_METHOD1(GetCurrentSpaceForGid, int64_t(gid_t));
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_MOCK_ARC_DISK_QUOTA_H_
