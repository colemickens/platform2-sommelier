// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "smbprovider/mount_manager.h"

namespace smbprovider {

class MountManagerTest : public testing::Test {
 public:
  MountManagerTest() = default;
  ~MountManagerTest() override = default;

 protected:
  MountManager manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MountManagerTest);
};

TEST_F(MountManagerTest, TestEmptyManager) {
  // Verify the state of an empty |MountManager|
  EXPECT_EQ(0, manager_.MountCount());
  EXPECT_FALSE(manager_.RemoveMount(0 /* mount_id */));
  EXPECT_EQ(0, manager_.MountCount());
  EXPECT_FALSE(manager_.IsAlreadyMounted(0 /* mount_id */));

  std::string full_path;
  EXPECT_FALSE(manager_.GetFullPath(0 /* mount_id */, "foo.txt", &full_path));
}

TEST_F(MountManagerTest, TestAddRemoveMount) {
  // Add a new mount.
  const std::string root_path = "smb://server/share";
  const int32_t mount_id = manager_.AddMount(root_path);

  // Verify the mount was added with a valid id.
  EXPECT_GE(mount_id, 0);
  EXPECT_EQ(1, manager_.MountCount());
  EXPECT_TRUE(manager_.IsAlreadyMounted(mount_id));

  // Verify the mount can be removed.
  EXPECT_TRUE(manager_.RemoveMount(mount_id));
  EXPECT_EQ(0, manager_.MountCount());
  EXPECT_FALSE(manager_.IsAlreadyMounted(mount_id));
}

TEST_F(MountManagerTest, TestAddThenRemoveWrongMount) {
  // Add a new mount.
  const std::string root_path = "smb://server/share";
  const int32_t mount_id = manager_.AddMount(root_path);

  // Verify RemoveMount fails with an invalid id and nothing is removed.
  const int32_t invalid_mount_id = mount_id + 1;
  EXPECT_FALSE(manager_.IsAlreadyMounted(invalid_mount_id));
  EXPECT_FALSE(manager_.RemoveMount(invalid_mount_id));
  EXPECT_EQ(1, manager_.MountCount());
  EXPECT_TRUE(manager_.IsAlreadyMounted(mount_id));

  // Verify the valid id can still be removed.
  EXPECT_TRUE(manager_.RemoveMount(mount_id));
  EXPECT_EQ(0, manager_.MountCount());
  EXPECT_FALSE(manager_.IsAlreadyMounted(mount_id));
}

TEST_F(MountManagerTest, TestAddRemoveMultipleMounts) {
  // For this test it doesn't matter if the same root is used for both
  // mounts.
  const std::string root_path = "smb://server/share";

  // Add two mounts and verify they were both added.
  const int32_t mount_id_1 = manager_.AddMount(root_path);
  const int32_t mount_id_2 = manager_.AddMount(root_path);
  EXPECT_EQ(2, manager_.MountCount());
  EXPECT_TRUE(manager_.IsAlreadyMounted(mount_id_1));
  EXPECT_TRUE(manager_.IsAlreadyMounted(mount_id_2));

  // Verify the ids are valid and different.
  EXPECT_GE(mount_id_1, 0);
  EXPECT_GE(mount_id_2, 0);
  EXPECT_NE(mount_id_1, mount_id_2);

  // Remove the second id, verify it is removed, and the first remains.
  EXPECT_TRUE(manager_.RemoveMount(mount_id_2));
  EXPECT_EQ(1, manager_.MountCount());
  EXPECT_FALSE(manager_.IsAlreadyMounted(mount_id_2));
  EXPECT_TRUE(manager_.IsAlreadyMounted(mount_id_1));

  // Remove the first id and verify it is also removed.
  EXPECT_TRUE(manager_.RemoveMount(mount_id_1));
  EXPECT_EQ(0, manager_.MountCount());
  EXPECT_FALSE(manager_.IsAlreadyMounted(mount_id_2));
}

TEST_F(MountManagerTest, TestGetFullPath) {
  // Add a new mount.
  const std::string root_path = "smb://server/share";
  const int32_t mount_id = manager_.AddMount(root_path);

  // Verify the full path is as expected.
  const std::string entry_path = "/foo/bar";
  const std::string expected_full_path = root_path + entry_path;
  std::string actual_full_path;
  EXPECT_TRUE(manager_.GetFullPath(mount_id, entry_path, &actual_full_path));
  EXPECT_EQ(expected_full_path, actual_full_path);
}

TEST_F(MountManagerTest, TestGetFullPathWithInvalidId) {
  // Add a new mount.
  const std::string root_path = "smb://server/share";
  const int32_t mount_id = manager_.AddMount(root_path);

  // Verify calling GetFullPath() with an invalid id fails.
  const int32_t invalid_mount_id = mount_id + 1;
  EXPECT_FALSE(manager_.IsAlreadyMounted(invalid_mount_id));
  std::string full_path;
  EXPECT_FALSE(manager_.GetFullPath(invalid_mount_id, "/foo/bar", &full_path));
}

TEST_F(MountManagerTest, TestGetFullPathMultipleMounts) {
  // Add two mounts with different roots.
  const std::string root_path_1 = "smb://server/share1";
  const std::string root_path_2 = "smb://server/share2";
  ASSERT_NE(root_path_1, root_path_2);
  const int32_t mount_id_1 = manager_.AddMount(root_path_1);
  const int32_t mount_id_2 = manager_.AddMount(root_path_2);

  // Verify correct ids map to the correct paths.
  std::string actual_full_path;
  const std::string entry_path = "/foo/bar";
  const std::string expected_full_path_1 = root_path_1 + entry_path;
  const std::string expected_full_path_2 = root_path_2 + entry_path;
  EXPECT_TRUE(manager_.GetFullPath(mount_id_1, entry_path, &actual_full_path));
  EXPECT_EQ(expected_full_path_1, actual_full_path);
  EXPECT_TRUE(manager_.GetFullPath(mount_id_2, entry_path, &actual_full_path));
  EXPECT_EQ(expected_full_path_2, actual_full_path);
}

TEST_F(MountManagerTest, TestGetRelativePath) {
  const std::string root_path = "smb://server/share1";
  const int32_t mount_id = manager_.AddMount(root_path);

  const std::string full_path = "smb://server/share1/animals/dog.jpg";
  const std::string expected_relative_path = "/animals/dog.jpg";

  EXPECT_EQ(expected_relative_path,
            manager_.GetRelativePath(mount_id, full_path));
}

TEST_F(MountManagerTest, TestGetRelativePathOnRoot) {
  const std::string root_path = "smb://server/share1";
  const int32_t mount_id = manager_.AddMount(root_path);

  const std::string full_path = "smb://server/share1/";
  const std::string expected_relative_path = "/";

  EXPECT_EQ(expected_relative_path,
            manager_.GetRelativePath(mount_id, full_path));
}

}  // namespace smbprovider
