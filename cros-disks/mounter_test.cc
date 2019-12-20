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
using testing::WithArg;

namespace cros_disks {

namespace {

const char kPath[] = "/mnt/foo/bar";

class MounterForTest : public Mounter {
 public:
  MounterForTest() : Mounter("fs_type") {}

  MOCK_METHOD(MountErrorType,
              MountImpl,
              (const std::string&, const base::FilePath&),
              (const));
  MOCK_METHOD(MountErrorType, UnmountImpl, (const base::FilePath&), (const));
  MOCK_METHOD(bool,
              CanMount,
              (const std::string&,
               const std::vector<std::string>&,
               base::FilePath*),
              (const, override));

  std::unique_ptr<MountPoint> Mount(const std::string& source,
                                    const base::FilePath& target_path,
                                    std::vector<std::string> options,
                                    MountErrorType* error) const override {
    *error = MountImpl(source, target_path);
    if (*error != MOUNT_ERROR_NONE) {
      return nullptr;
    }

    return std::make_unique<MockMountPoint>(target_path, this);
  }

 private:
  class MockMountPoint : public MountPoint {
   public:
    explicit MockMountPoint(const base::FilePath& path,
                            const MounterForTest* mounter)
        : MountPoint(path), mounter_(mounter) {}
    ~MockMountPoint() override { DestructorUnmount(); }

   protected:
    MountErrorType UnmountImpl() override {
      return mounter_->UnmountImpl(path());
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

  MOCK_METHOD(std::unique_ptr<MountPoint>,
              Mount,
              (const std::string&,
               const base::FilePath&,
               std::vector<std::string>,
               MountErrorType*),
              (const, override));
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
  const base::FilePath kMountPath = base::FilePath("/mnt");
  const std::vector<std::string> options;

  MounterCompatForTest mounter("fstype", "foo", kMountPath, {});
  EXPECT_CALL(mounter, Mount("foo", kMountPath, options, _))
      .WillOnce(WithArg<3>([kMountPath](MountErrorType* error) {
        *error = MOUNT_ERROR_NONE;
        return MountPoint::CreateLeaking(kMountPath);
      }));
  EXPECT_EQ(MOUNT_ERROR_NONE, mounter.MountOld());
}

TEST(MounterCompatTest, MountFail) {
  const base::FilePath kMountPath = base::FilePath("/mnt");
  const std::vector<std::string> options;

  MounterCompatForTest mounter("fstype", "foo", kMountPath, {});
  EXPECT_CALL(mounter, Mount("foo", kMountPath, options, _))
      .WillOnce(WithArg<3>([](MountErrorType* error) {
        *error = MOUNT_ERROR_UNKNOWN;
        return nullptr;
      }));
  EXPECT_EQ(MOUNT_ERROR_UNKNOWN, mounter.MountOld());
}

}  // namespace cros_disks
