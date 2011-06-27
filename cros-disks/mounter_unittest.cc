// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "cros-disks/mounter.h"
#include "cros-disks/mount-options.h"

namespace cros_disks {

using ::testing::Return;

// A mock mounter class for testing the mounter base class.
class MounterUnderTest : public Mounter {
 public:
  MounterUnderTest(const std::string& source_path,
      const std::string& target_path,
      const std::string& filesystem_type,
      const MountOptions& mount_options)
    : Mounter(source_path, target_path, filesystem_type, mount_options) {
  }

  // Mocks mount implementation.
  MOCK_METHOD0(MountImpl, bool());
};

class MounterTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    source_path_ = "/dev/sdb1";
    target_path_ = "/media/disk";
    filesystem_type_ = "vfat";
  }

  std::string filesystem_type_;
  std::string source_path_;
  std::string target_path_;
  MountOptions mount_options_;
  std::vector<std::string> options_;
};

TEST_F(MounterTest, MountReadOnlySucceeded) {
  mount_options_.Initialize(options_, false, "", "");
  MounterUnderTest mounter(source_path_, target_path_,
      filesystem_type_, mount_options_);

  EXPECT_CALL(mounter, MountImpl())
    .Times(1)
    .WillOnce(Return(true));
  EXPECT_TRUE(mounter.mount_options().IsReadOnlyOptionSet());
  EXPECT_TRUE(mounter.Mount());
}

TEST_F(MounterTest, MountReadOnlyFailed) {
  mount_options_.Initialize(options_, false, "", "");
  MounterUnderTest mounter(source_path_, target_path_,
      filesystem_type_, mount_options_);

  EXPECT_CALL(mounter, MountImpl())
    .Times(1)
    .WillOnce(Return(false));
  EXPECT_TRUE(mounter.mount_options().IsReadOnlyOptionSet());
  EXPECT_FALSE(mounter.Mount());
}

TEST_F(MounterTest, MountReadWriteSucceeded) {
  options_.push_back("rw");
  mount_options_.Initialize(options_, false, "", "");
  MounterUnderTest mounter(source_path_, target_path_,
      filesystem_type_, mount_options_);

  EXPECT_CALL(mounter, MountImpl())
    .Times(1)
    .WillOnce(Return(true));
  EXPECT_FALSE(mounter.mount_options().IsReadOnlyOptionSet());
  EXPECT_TRUE(mounter.Mount());
}

TEST_F(MounterTest, MountReadWriteFailedButReadOnlySucceeded) {
  options_.push_back("rw");
  mount_options_.Initialize(options_, false, "", "");
  MounterUnderTest mounter(source_path_, target_path_,
      filesystem_type_, mount_options_);

  EXPECT_CALL(mounter, MountImpl())
    .Times(2)
    .WillOnce(Return(false))
    .WillOnce(Return(true));
  EXPECT_FALSE(mounter.mount_options().IsReadOnlyOptionSet());
  EXPECT_TRUE(mounter.Mount());
  EXPECT_TRUE(mounter.mount_options().IsReadOnlyOptionSet());
}

TEST_F(MounterTest, MountReadWriteAndReadOnlyFailed) {
  options_.push_back("rw");
  mount_options_.Initialize(options_, false, "", "");
  MounterUnderTest mounter(source_path_, target_path_,
      filesystem_type_, mount_options_);

  EXPECT_CALL(mounter, MountImpl())
    .Times(2)
    .WillOnce(Return(false))
    .WillOnce(Return(false));
  EXPECT_FALSE(mounter.mount_options().IsReadOnlyOptionSet());
  EXPECT_FALSE(mounter.Mount());
  EXPECT_TRUE(mounter.mount_options().IsReadOnlyOptionSet());
}

}  // namespace cros_disks
