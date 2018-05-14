// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/mounter.h"

#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "cros-disks/mount_options.h"

using std::string;
using std::vector;
using testing::Return;

namespace cros_disks {

// A mock mounter class for testing the mounter base class.
class MounterUnderTest : public Mounter {
 public:
  MounterUnderTest(const string& source_path,
                   const string& target_path,
                   const string& filesystem_type,
                   const MountOptions& mount_options)
      : Mounter(source_path, target_path, filesystem_type, mount_options) {}

  // Mocks mount implementation.
  MOCK_METHOD0(MountImpl, MountErrorType());
};

class MounterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    source_path_ = "/dev/sdb1";
    target_path_ = "/media/disk";
    filesystem_type_ = "vfat";
  }

  string filesystem_type_;
  string source_path_;
  string target_path_;
  MountOptions mount_options_;
  vector<string> options_;
};

TEST_F(MounterTest, MountReadOnlySucceeded) {
  mount_options_.Initialize(options_, false, "", "");
  MounterUnderTest mounter(source_path_, target_path_, filesystem_type_,
                           mount_options_);

  EXPECT_CALL(mounter, MountImpl()).WillOnce(Return(MOUNT_ERROR_NONE));
  EXPECT_TRUE(mounter.mount_options().IsReadOnlyOptionSet());
  EXPECT_EQ(MOUNT_ERROR_NONE, mounter.Mount());
}

TEST_F(MounterTest, MountReadWriteSucceeded) {
  options_.push_back("rw");
  mount_options_.Initialize(options_, false, "", "");
  MounterUnderTest mounter(source_path_, target_path_, filesystem_type_,
                           mount_options_);

  EXPECT_CALL(mounter, MountImpl()).WillOnce(Return(MOUNT_ERROR_NONE));
  EXPECT_FALSE(mounter.mount_options().IsReadOnlyOptionSet());
  EXPECT_EQ(MOUNT_ERROR_NONE, mounter.Mount());
}

TEST_F(MounterTest, MountFailed) {
  mount_options_.Initialize(options_, false, "", "");
  MounterUnderTest mounter(source_path_, target_path_, filesystem_type_,
                           mount_options_);

  EXPECT_CALL(mounter, MountImpl())
      .WillOnce(Return(MOUNT_ERROR_INTERNAL));
  EXPECT_EQ(MOUNT_ERROR_INTERNAL, mounter.Mount());
}

}  // namespace cros_disks
