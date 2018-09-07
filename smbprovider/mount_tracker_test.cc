// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <memory>

#include <base/bind.h>
#include <base/macros.h>
#include <base/test/simple_test_tick_clock.h>
#include <gtest/gtest.h>

#include "smbprovider/fake_samba_interface.h"
#include "smbprovider/fake_samba_proxy.h"
#include "smbprovider/mount_manager.h"
#include "smbprovider/mount_tracker.h"
#include "smbprovider/smbprovider_test_helper.h"
#include "smbprovider/temp_file_manager.h"

namespace smbprovider {

namespace {

std::unique_ptr<SambaInterface> SambaInterfaceFactoryFunction(
    FakeSambaInterface* fake_samba) {
  return std::make_unique<FakeSambaProxy>(fake_samba);
}

}  // namespace

class MountTrackerTest : public testing::Test {
 public:
  using SambaInterfaceFactory =
      base::Callback<std::unique_ptr<SambaInterface>()>;

  MountTrackerTest() {
    auto tick_clock = std::make_unique<base::SimpleTestTickClock>();

    auto fake_samba_ = std::make_unique<FakeSambaInterface>();
    samba_interface_factory_ =
        base::Bind(&SambaInterfaceFactoryFunction, fake_samba_.get());

    mount_tracker_ = std::make_unique<MountTracker>(std::move(tick_clock));
  }

  ~MountTrackerTest() override = default;

  bool AddMountWithEmptyCredential(const std::string& root_path,
                                   int32_t* mount_id) {
    SmbCredential credential("" /* workgroup */, "" /* username */,
                             GetEmptyPassword());

    return mount_tracker_->AddMount(root_path, std::move(credential),
                                    CreateSambaInterface(), mount_id);
  }

  std::unique_ptr<SambaInterface> CreateSambaInterface() {
    return samba_interface_factory_.Run();
  }

 protected:
  std::unique_ptr<MountTracker> mount_tracker_;
  TempFileManager temp_files_;
  SambaInterfaceFactory samba_interface_factory_;

 private:
  base::ScopedFD WriteEmptyPasswordToFile() {
    return WritePasswordToFile(&temp_files_, "" /* password */);
  }

  std::unique_ptr<password_provider::Password> GetEmptyPassword() {
    return GetPassword(WriteEmptyPasswordToFile());
  }

  DISALLOW_COPY_AND_ASSIGN(MountTrackerTest);
};

TEST_F(MountTrackerTest, TestNegativeMounts) {
  const std::string root_path = "smb://server/share";
  const int32_t mount_id = 1;

  EXPECT_FALSE(mount_tracker_->IsAlreadyMounted(root_path));
  EXPECT_FALSE(mount_tracker_->IsAlreadyMounted(mount_id));
}

TEST_F(MountTrackerTest, TestAddMount) {
  const std::string root_path = "smb://server/share";

  EXPECT_FALSE(mount_tracker_->IsAlreadyMounted(root_path));
  int32_t mount_id;
  EXPECT_TRUE(AddMountWithEmptyCredential(root_path, &mount_id));

  EXPECT_EQ(1, mount_tracker_->MountCount());

  EXPECT_TRUE(mount_tracker_->IsAlreadyMounted(root_path));
  EXPECT_TRUE(mount_tracker_->IsAlreadyMounted(mount_id));
}

TEST_F(MountTrackerTest, TestAddSameMount) {
  const std::string root_path = "smb://server/share";

  EXPECT_FALSE(mount_tracker_->IsAlreadyMounted(root_path));
  int32_t mount_id;
  EXPECT_TRUE(AddMountWithEmptyCredential(root_path, &mount_id));

  EXPECT_TRUE(mount_tracker_->IsAlreadyMounted(root_path));
  EXPECT_TRUE(mount_tracker_->IsAlreadyMounted(mount_id));
  EXPECT_EQ(1, mount_tracker_->MountCount());

  // Ensure IsAlreadyMounted is working after adding a mount.
  const std::string root_path2 = "smb://server/share2";
  EXPECT_FALSE(mount_tracker_->IsAlreadyMounted(root_path2));

  const int32_t mount_id2 = 9;
  EXPECT_FALSE(mount_tracker_->IsAlreadyMounted(mount_id2));

  EXPECT_FALSE(AddMountWithEmptyCredential(root_path, &mount_id));

  EXPECT_TRUE(mount_tracker_->IsAlreadyMounted(root_path));
  EXPECT_TRUE(mount_tracker_->IsAlreadyMounted(mount_id));

  EXPECT_FALSE(AddMountWithEmptyCredential(root_path, &mount_id));

  EXPECT_EQ(1, mount_tracker_->MountCount());
}

TEST_F(MountTrackerTest, TestMountCount) {
  const std::string root_path = "smb://server/share1";
  const std::string root_path2 = "smb://server/share2";

  EXPECT_EQ(0, mount_tracker_->MountCount());

  int32_t mount_id1;
  EXPECT_TRUE(AddMountWithEmptyCredential(root_path, &mount_id1));

  EXPECT_EQ(1, mount_tracker_->MountCount());

  int32_t mount_id2;
  EXPECT_TRUE(AddMountWithEmptyCredential(root_path2, &mount_id2));

  EXPECT_EQ(2, mount_tracker_->MountCount());
  EXPECT_NE(mount_id1, mount_id2);
}

TEST_F(MountTrackerTest, TestAddMultipleDifferentMountId) {
  const std::string root_path1 = "smb://server/share1";
  int32_t mount_id1;
  EXPECT_TRUE(AddMountWithEmptyCredential(root_path1, &mount_id1));

  const std::string root_path2 = "smb://server/share2";
  int32_t mount_id2;
  EXPECT_TRUE(AddMountWithEmptyCredential(root_path2, &mount_id2));

  EXPECT_GE(mount_id1, 0);
  EXPECT_GE(mount_id2, 0);
  EXPECT_NE(mount_id1, mount_id2);
}

}  // namespace smbprovider
