// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <memory>

#include <base/macros.h>
#include <gtest/gtest.h>

#include "smbprovider/mount_tracker.h"

namespace smbprovider {

class MountTrackerTest : public testing::Test {
 public:
  MountTrackerTest() : mount_tracker_(std::make_unique<MountTracker>()) {}

  ~MountTrackerTest() override = default;

 protected:
  std::unique_ptr<MountTracker> mount_tracker_;

  DISALLOW_COPY_AND_ASSIGN(MountTrackerTest);
};

TEST_F(MountTrackerTest, TestNegativeMounts) {
  const std::string root_path = "smb://server/share";
  const int32_t mount_id = 1;

  EXPECT_FALSE(mount_tracker_->IsAlreadyMounted(root_path));
  EXPECT_FALSE(mount_tracker_->IsAlreadyMounted(mount_id));
}

}  // namespace smbprovider
