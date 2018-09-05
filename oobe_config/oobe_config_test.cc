// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "oobe_config/oobe_config.h"

#include <base/files/file_path.h>
#include <base/files/scoped_temp_dir.h>
#include <gtest/gtest.h>
#include <string>

#include "oobe_config/oobe_config_constants.h"

namespace oobe_config {

TEST(OobeConfigTest, SaveAndRestoreTest) {
  oobe_config::OobeConfig oobe_config;

  // Creating pre-rollback state.
  base::ScopedTempDir tempdir_before;
  ASSERT_TRUE(tempdir_before.CreateUniqueTempDir());
  oobe_config.set_prefix_path_for_testing(tempdir_before.GetPath());

  oobe_config.WriteFile(kInstallAttributesPath, "install_attributes");
  oobe_config.WriteFile(kOwnerKeyPath, "owner");
  oobe_config.WriteFile(kPolicyPath, "policy0");
  oobe_config.WriteFile(kPolicyDotOnePathForTesting, "policy1");
  oobe_config.WriteFile(kShillDefaultProfilePath, "shill");

  // Saving rollback data.
  ASSERT_TRUE(oobe_config.UnencryptedRollbackSave());
  std::string rollback_data;
  ASSERT_TRUE(oobe_config.ReadFile(kRollbackDataPath, &rollback_data));
  EXPECT_FALSE(rollback_data.empty());

  // Simulate powerwash and only preserve rollback_data by creating new temp
  // dir.
  base::ScopedTempDir tempdir_after;
  ASSERT_TRUE(tempdir_after.CreateUniqueTempDir());
  oobe_config.set_prefix_path_for_testing(tempdir_after.GetPath());

  // Verify that we don't have any remaining files.
  std::string tmp_data = "x";
  ASSERT_FALSE(oobe_config.ReadFile(kRollbackDataPath, &tmp_data));
  EXPECT_TRUE(tmp_data.empty());
  // Preserving rollback data.
  ASSERT_TRUE(oobe_config.WriteFile(kRollbackDataPath, rollback_data));

  // Restore data.
  EXPECT_TRUE(oobe_config.UnencryptedRollbackRestore());

  // Verify that the config files are restored.
  std::string file_content;
  EXPECT_TRUE(oobe_config.ReadFile(kInstallAttributesPath, &file_content));
  EXPECT_EQ("install_attributes", file_content);
  EXPECT_TRUE(oobe_config.ReadFile(kOwnerKeyPath, &file_content));
  EXPECT_EQ("owner", file_content);
  EXPECT_TRUE(oobe_config.ReadFile(kPolicyPath, &file_content));
  EXPECT_EQ("policy0", file_content);
  EXPECT_TRUE(oobe_config.ReadFile(kPolicyDotOnePathForTesting, &file_content));
  EXPECT_EQ("policy1", file_content);
  EXPECT_TRUE(oobe_config.ReadFile(kShillDefaultProfilePath, &file_content));
  EXPECT_EQ("shill", file_content);
}

}  // namespace oobe_config
