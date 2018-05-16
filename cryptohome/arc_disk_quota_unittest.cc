// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/arc_disk_quota.h"
#include "cryptohome/mock_platform.h"

#include <memory>
#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/quota.h>
#include <sys/types.h>

using ::testing::_;
using ::testing::DoAll;
using ::testing::Ne;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::SetArrayArgument;

namespace cryptohome {

namespace {

constexpr base::FilePath::CharType kHome[] = "/home";
constexpr base::FilePath::CharType kDev[] = "/dev/mmcblk0p1";

}  // namespace

class ArcDiskQuotaTest : public ::testing::Test {
 public:
  ArcDiskQuotaTest()
      : arc_disk_quota_(&platform_,
                        base::FilePath(kHome),
                        base::FilePath(".shadow"),
                        base::FilePath("mount/root/android-data")) {}
  ~ArcDiskQuotaTest() override {}

 protected:
  MockPlatform platform_;
  ArcDiskQuota arc_disk_quota_;

  DISALLOW_COPY_AND_ASSIGN(ArcDiskQuotaTest);
};

TEST_F(ArcDiskQuotaTest, QuotaIsSupported) {
  EXPECT_CALL(platform_, FindFilesystemDevice(base::FilePath(kHome), _))
      .WillOnce(DoAll(SetArgPointee<1>(kDev), Return(true)));

  EXPECT_CALL(platform_, GetQuotaCurrentSpaceForUid(base::FilePath(kDev), 0))
      .WillOnce(Return(0));

  // Exactly 1 Android user.
  MockFileEnumerator file_enumerator;
  EXPECT_CALL(platform_,
              GetFileEnumerator(base::FilePath("/home/.shadow"), false,
                                base::FileEnumerator::DIRECTORIES))
      .WillOnce(Return(&file_enumerator));

  EXPECT_CALL(file_enumerator, Next())
      .WillOnce(Return(base::FilePath("/home/.shadow/1")))
      .WillOnce(Return(base::FilePath("/home/.shadow/2")))
      .WillOnce(Return(base::FilePath()));
  EXPECT_CALL(
      platform_,
      FileExists(base::FilePath("/home/.shadow/1/mount/root/android-data")))
      .WillOnce(Return(false));
  EXPECT_CALL(
      platform_,
      FileExists(base::FilePath("/home/.shadow/2/mount/root/android-data")))
      .WillOnce(Return(true));

  arc_disk_quota_.Initialize();
  EXPECT_EQ(true, arc_disk_quota_.IsQuotaSupported());
}

TEST_F(ArcDiskQuotaTest, QuotaIsNotSupported_NoDevice) {
  EXPECT_CALL(platform_, FindFilesystemDevice(base::FilePath(kHome), _))
      .WillOnce(DoAll(SetArgPointee<1>(""), Return(false)));

  arc_disk_quota_.Initialize();
  EXPECT_EQ(false, arc_disk_quota_.IsQuotaSupported());
}

TEST_F(ArcDiskQuotaTest, QuotaIsNotSupported_NoQuotaMountedDevice) {
  EXPECT_CALL(platform_, FindFilesystemDevice(base::FilePath(kHome), _))
      .WillOnce(DoAll(SetArgPointee<1>(kDev), Return(true)));

  EXPECT_CALL(platform_, GetQuotaCurrentSpaceForUid(base::FilePath(kDev), 0))
      .WillOnce(Return(-1));

  arc_disk_quota_.Initialize();
  EXPECT_EQ(false, arc_disk_quota_.IsQuotaSupported());
}

TEST_F(ArcDiskQuotaTest, QuotaIsNotSupported_MultipleAndroidUser) {
  EXPECT_CALL(platform_, FindFilesystemDevice(base::FilePath(kHome), _))
      .WillOnce(DoAll(SetArgPointee<1>(kDev), Return(true)));

  EXPECT_CALL(platform_, GetQuotaCurrentSpaceForUid(base::FilePath(kDev), 0))
      .WillOnce(Return(0));

  // Multiple Android user.
  MockFileEnumerator file_enumerator;
  EXPECT_CALL(platform_,
              GetFileEnumerator(base::FilePath("/home/.shadow"), false,
                                base::FileEnumerator::DIRECTORIES))
      .WillOnce(Return(&file_enumerator));

  EXPECT_CALL(file_enumerator, Next())
      .WillOnce(Return(base::FilePath("/home/.shadow/1")))
      .WillOnce(Return(base::FilePath("/home/.shadow/2")))
      .WillOnce(Return(base::FilePath()));
  EXPECT_CALL(
      platform_,
      FileExists(base::FilePath("/home/.shadow/1/mount/root/android-data")))
      .WillOnce(Return(true));
  EXPECT_CALL(
      platform_,
      FileExists(base::FilePath("/home/.shadow/2/mount/root/android-data")))
      .WillOnce(Return(true));

  arc_disk_quota_.Initialize();
  EXPECT_EQ(false, arc_disk_quota_.IsQuotaSupported());
}

TEST_F(ArcDiskQuotaTest, GetCurrentSpaceForUid_Succeeds) {
  EXPECT_CALL(platform_, FindFilesystemDevice(base::FilePath(kHome), _))
      .WillOnce(DoAll(SetArgPointee<1>(kDev), Return(true)));

  EXPECT_CALL(platform_, GetQuotaCurrentSpaceForUid(base::FilePath(kDev), 0))
      .WillOnce(Return(0));

  EXPECT_CALL(platform_, GetQuotaCurrentSpaceForUid(base::FilePath(kDev), 10))
      .WillOnce(Return(5));

  arc_disk_quota_.Initialize();
  EXPECT_EQ(5, arc_disk_quota_.GetCurrentSpaceForUid(10));
}

TEST_F(ArcDiskQuotaTest, GetCurrentSpaceForUid_NoDevice) {
  EXPECT_CALL(platform_, FindFilesystemDevice(base::FilePath(kHome), _))
      .WillOnce(DoAll(SetArgPointee<1>(""), Return(false)));

  EXPECT_CALL(platform_, GetQuotaCurrentSpaceForUid(_, _)).Times(0);

  arc_disk_quota_.Initialize();
  EXPECT_EQ(-1, arc_disk_quota_.GetCurrentSpaceForUid(10));
}

TEST_F(ArcDiskQuotaTest, GetCurrentSpaceForUid_NoQuotaMountedDevice) {
  EXPECT_CALL(platform_, FindFilesystemDevice(base::FilePath(kHome), _))
      .WillOnce(DoAll(SetArgPointee<1>(kDev), Return(true)));

  EXPECT_CALL(platform_, GetQuotaCurrentSpaceForUid(base::FilePath(kDev), 0))
      .WillOnce(Return(-1));

  EXPECT_CALL(platform_,
              GetQuotaCurrentSpaceForUid(Ne(base::FilePath(kDev)), Ne(0)))
      .Times(0);

  arc_disk_quota_.Initialize();
  EXPECT_EQ(-1, arc_disk_quota_.GetCurrentSpaceForUid(10));
}

TEST_F(ArcDiskQuotaTest, GetCurrentSpaceForUid_QuotactlFails) {
  EXPECT_CALL(platform_, FindFilesystemDevice(base::FilePath(kHome), _))
      .WillOnce(DoAll(SetArgPointee<1>(kDev), Return(true)));

  EXPECT_CALL(platform_, GetQuotaCurrentSpaceForUid(base::FilePath(kDev), 0))
      .WillOnce(Return(0));

  EXPECT_CALL(platform_, GetQuotaCurrentSpaceForUid(base::FilePath(kDev), 10))
      .WillOnce(Return(-1));

  arc_disk_quota_.Initialize();
  EXPECT_EQ(-1, arc_disk_quota_.GetCurrentSpaceForUid(10));
}

TEST_F(ArcDiskQuotaTest, GetCurrentSpaceForGid_Succeeds) {
  EXPECT_CALL(platform_, FindFilesystemDevice(base::FilePath(kHome), _))
      .WillOnce(DoAll(SetArgPointee<1>(kDev), Return(true)));

  EXPECT_CALL(platform_, GetQuotaCurrentSpaceForUid(base::FilePath(kDev), 0))
      .WillOnce(Return(0));

  EXPECT_CALL(platform_, GetQuotaCurrentSpaceForGid(base::FilePath(kDev), 10))
      .WillOnce(Return(5));

  arc_disk_quota_.Initialize();
  EXPECT_EQ(5, arc_disk_quota_.GetCurrentSpaceForGid(10));
}

TEST_F(ArcDiskQuotaTest, GetCurrentSpaceForGid_NoDevice) {
  EXPECT_CALL(platform_, FindFilesystemDevice(base::FilePath(kHome), _))
      .WillOnce(DoAll(SetArgPointee<1>(""), Return(false)));

  EXPECT_CALL(platform_, GetQuotaCurrentSpaceForGid(_, _)).Times(0);

  arc_disk_quota_.Initialize();
  EXPECT_EQ(-1, arc_disk_quota_.GetCurrentSpaceForGid(10));
}

TEST_F(ArcDiskQuotaTest, GetCurrentSpaceForGid_NoQuotaMountedDevice) {
  EXPECT_CALL(platform_, FindFilesystemDevice(base::FilePath(kHome), _))
      .WillOnce(DoAll(SetArgPointee<1>(kDev), Return(true)));

  EXPECT_CALL(platform_, GetQuotaCurrentSpaceForUid(base::FilePath(kDev), 0))
      .WillOnce(Return(-1));

  EXPECT_CALL(platform_,
              GetQuotaCurrentSpaceForUid(Ne(base::FilePath(kDev)), Ne(0)))
      .Times(0);

  arc_disk_quota_.Initialize();
  EXPECT_EQ(-1, arc_disk_quota_.GetCurrentSpaceForGid(10));
}

TEST_F(ArcDiskQuotaTest, GetCurrentSpaceForGid_QuotactlFails) {
  EXPECT_CALL(platform_, FindFilesystemDevice(base::FilePath(kHome), _))
      .WillOnce(DoAll(SetArgPointee<1>(kDev), Return(true)));

  EXPECT_CALL(platform_, GetQuotaCurrentSpaceForUid(base::FilePath(kDev), 0))
      .WillOnce(Return(0));

  EXPECT_CALL(platform_, GetQuotaCurrentSpaceForGid(base::FilePath(kDev), 10))
      .WillOnce(Return(-1));

  arc_disk_quota_.Initialize();
  EXPECT_EQ(-1, arc_disk_quota_.GetCurrentSpaceForGid(10));
}

}  // namespace cryptohome
