// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/mount_point.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "cros-disks/mounter.h"

namespace cros_disks {

namespace {

using testing::_;
using testing::Return;

class MockUnmounter : public Unmounter {
 public:
  MountErrorType Unmount(const MountPoint& mountpoint) override {
    return UnmountImpl(mountpoint.path());
  }
  MOCK_METHOD1(UnmountImpl, MountErrorType(const base::FilePath& path));
};

}  // namespace

TEST(MountPointTest, Unmount) {
  auto unmounter = std::make_unique<MockUnmounter>();
  EXPECT_CALL(*unmounter, UnmountImpl(base::FilePath("/mnt/path")))
      .WillOnce(Return(MountErrorType::MOUNT_ERROR_INVALID_ARCHIVE))
      .WillOnce(Return(MountErrorType::MOUNT_ERROR_NONE));
  MountPoint mp(base::FilePath("/mnt/path"), std::move(unmounter));
  EXPECT_EQ(MountErrorType::MOUNT_ERROR_INVALID_ARCHIVE, mp.Unmount());
  EXPECT_EQ(MountErrorType::MOUNT_ERROR_NONE, mp.Unmount());
  EXPECT_EQ(MountErrorType::MOUNT_ERROR_PATH_NOT_MOUNTED, mp.Unmount());
}

TEST(MountPointTest, UnmountOnDestroy) {
  auto unmounter = std::make_unique<MockUnmounter>();
  EXPECT_CALL(*unmounter, UnmountImpl(base::FilePath("/mnt/path")))
      .WillOnce(Return(MountErrorType::MOUNT_ERROR_INVALID_ARCHIVE));
  MountPoint mp(base::FilePath("/mnt/path"), std::move(unmounter));
}

TEST(MountPointTest, LeakMount) {
  auto unmounter = std::make_unique<MockUnmounter>();
  EXPECT_CALL(*unmounter, UnmountImpl(_)).Times(0);
  MountPoint mp(base::FilePath("/mnt/path"), std::move(unmounter));
  mp.Release();
}

}  // namespace cros_disks
