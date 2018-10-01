// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "oobe_config/load_oobe_config_rollback.h"

#include <memory>
#include <string>

#include <base/files/scoped_temp_dir.h>
#include <gtest/gtest.h>

#include "oobe_config/oobe_config.h"
#include "oobe_config/rollback_constants.h"

using base::ScopedTempDir;
using std::string;
using std::unique_ptr;

namespace oobe_config {

class LoadOobeConfigRollbackTest : public ::testing::Test {
 protected:
  void SetUp() override {
    oobe_config_ = std::make_unique<OobeConfig>();
    ASSERT_TRUE(fake_root_dir_.CreateUniqueTempDir());
    oobe_config_->set_prefix_path_for_testing(fake_root_dir_.GetPath());
    load_config_ = std::make_unique<LoadOobeConfigRollback>(
        oobe_config_.get(), /*allow_unencrypted=*/true,
        /*execute_commands=*/false);
  }

  base::ScopedTempDir fake_root_dir_;

  unique_ptr<LoadOobeConfigRollback> load_config_;
  unique_ptr<OobeConfig> oobe_config_;
};

TEST_F(LoadOobeConfigRollbackTest, SimpleTest) {
  string config, enrollment_domain;
  EXPECT_FALSE(load_config_->GetOobeConfigJson(&config, &enrollment_domain));
}

TEST_F(LoadOobeConfigRollbackTest, CheckNotRollbackStages) {
  EXPECT_FALSE(load_config_->CheckFirstStage());
  EXPECT_FALSE(load_config_->CheckSecondStage());
  EXPECT_FALSE(load_config_->CheckThirdStage());
}

TEST_F(LoadOobeConfigRollbackTest, CheckFirstStage) {
  oobe_config_->WriteFile(kUnencryptedStatefulRollbackDataPath, "");
  EXPECT_TRUE(load_config_->CheckFirstStage());
  EXPECT_FALSE(load_config_->CheckSecondStage());
  EXPECT_FALSE(load_config_->CheckThirdStage());
}

TEST_F(LoadOobeConfigRollbackTest, CheckSecondStage) {
  oobe_config_->WriteFile(kFirstStageCompletedFile, "");
  // Second stage will delete this.
  oobe_config_->WriteFile(kUnencryptedStatefulRollbackDataPath, "");
  // First stage already created this.
  oobe_config_->WriteFile(kEncryptedStatefulRollbackDataPath, "");
  EXPECT_FALSE(load_config_->CheckFirstStage());
  EXPECT_TRUE(load_config_->CheckSecondStage());
  EXPECT_FALSE(load_config_->CheckThirdStage());
}

TEST_F(LoadOobeConfigRollbackTest, CheckThirdStage) {
  oobe_config_->WriteFile(kFirstStageCompletedFile, "");
  oobe_config_->WriteFile(kSecondStageCompletedFile, "");
  // Third stage needs this.
  oobe_config_->WriteFile(kEncryptedStatefulRollbackDataPath, "");
  EXPECT_FALSE(load_config_->CheckFirstStage());
  EXPECT_FALSE(load_config_->CheckSecondStage());
  EXPECT_TRUE(load_config_->CheckThirdStage());
}

}  // namespace oobe_config
