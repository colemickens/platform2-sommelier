// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/mounter.h"

#include <string>
#include <utility>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "cros-disks/mount_options.h"
#include "cros-disks/mount_point.h"
#include "cros-disks/platform.h"

using testing::_;
using testing::Return;

namespace cros_disks {

namespace {

const char kPath[] = "/mnt/foo/bar";

class MounterForTest : public Mounter {
 public:
  MounterForTest() : Mounter("fs_type") {}

  MOCK_CONST_METHOD2(MountImpl,
                     MountErrorType(const std::string& source,
                                    const base::FilePath& target_path));
  MOCK_CONST_METHOD1(UnmountImpl, MountErrorType(const base::FilePath& path));
  MOCK_CONST_METHOD3(CanMount,
                     bool(const std::string& source,
                          const std::vector<std::string>& options,
                          base::FilePath* suggested_dir_name));

  std::unique_ptr<MountPoint> Mount(const std::string& source,
                                    const base::FilePath& target_path,
                                    std::vector<std::string> options,
                                    MountErrorType* error) const override {
    *error = MountImpl(source, target_path);
    if (*error != MOUNT_ERROR_NONE) {
      return nullptr;
    }

    return std::make_unique<MountPoint>(target_path,
                                        std::make_unique<MockUnmounter>(this));
  }

 private:
  class MockUnmounter : public Unmounter {
   public:
    explicit MockUnmounter(const MounterForTest* mounter) : mounter_(mounter) {}

    MountErrorType Unmount(const MountPoint& mountpoint) override {
      return mounter_->UnmountImpl(mountpoint.path());
    }

   private:
    const MounterForTest* const mounter_;
  };
};

class MounterCompatForTest : public MounterCompat {
 public:
  MounterCompatForTest(const std::string& filesystem_type,
                       const std::string& source,
                       const base::FilePath& target_path,
                       const MountOptions& mount_options)
      : MounterCompat(filesystem_type, source, target_path, mount_options) {}

  MOCK_CONST_METHOD0(MountImpl, MountErrorType());
};

}  // namespace

TEST(MounterTest, Basics) {
  MounterForTest mounter;
  EXPECT_CALL(mounter, MountImpl("src", base::FilePath(kPath)))
      .WillOnce(Return(MOUNT_ERROR_NONE));

  MountErrorType error = MOUNT_ERROR_NONE;
  auto mount = mounter.Mount("src", base::FilePath(kPath), {}, &error);
  EXPECT_TRUE(mount);
  EXPECT_EQ(MOUNT_ERROR_NONE, error);

  EXPECT_CALL(mounter, UnmountImpl(base::FilePath(kPath)))
      .WillOnce(Return(MOUNT_ERROR_INVALID_ARCHIVE))
      .WillOnce(Return(MOUNT_ERROR_NONE));
  EXPECT_EQ(MOUNT_ERROR_INVALID_ARCHIVE, mount->Unmount());
}

TEST(MounterTest, Leaking) {
  MounterForTest mounter;
  EXPECT_CALL(mounter, MountImpl("src", base::FilePath(kPath)))
      .WillOnce(Return(MOUNT_ERROR_NONE));

  MountErrorType error = MOUNT_ERROR_NONE;
  auto mount = mounter.Mount("src", base::FilePath(kPath), {}, &error);
  EXPECT_TRUE(mount);
  EXPECT_EQ(MOUNT_ERROR_NONE, error);

  EXPECT_CALL(mounter, UnmountImpl(_)).Times(0);
  mount->Release();
}

TEST(MounterCompatTest, Properties) {
  MounterCompatForTest mounter("fstype", "foo", base::FilePath("/mnt"), {});
  EXPECT_EQ("fstype", mounter.filesystem_type());
  EXPECT_EQ("foo", mounter.source());
  EXPECT_EQ("/mnt", mounter.target_path().value());
}

TEST(MounterCompatTest, MountSuccess) {
  MounterCompatForTest mounter("fstype", "foo", base::FilePath("/mnt"), {});
  EXPECT_CALL(mounter, MountImpl()).WillOnce(Return(MOUNT_ERROR_NONE));
  EXPECT_EQ(MOUNT_ERROR_NONE, mounter.Mount());
}

TEST(MounterCompatTest, MountFail) {
  MounterCompatForTest mounter("fstype", "foo", base::FilePath("/mnt"), {});
  EXPECT_CALL(mounter, MountImpl()).WillOnce(Return(MOUNT_ERROR_UNKNOWN));
  EXPECT_EQ(MOUNT_ERROR_UNKNOWN, mounter.Mount());
}

}  // namespace cros_disks
