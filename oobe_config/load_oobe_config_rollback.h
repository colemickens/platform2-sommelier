// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OOBE_CONFIG_LOAD_OOBE_CONFIG_ROLLBACK_H_
#define OOBE_CONFIG_LOAD_OOBE_CONFIG_ROLLBACK_H_

#include "oobe_config/load_oobe_config_interface.h"

#include <memory>
#include <string>

#include <base/files/file_path.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

namespace oobe_config {

class OobeConfig;
class RollbackData;

// An object of this class has the responsibility of loading the oobe config
// file after rollback.
class LoadOobeConfigRollback : public LoadOobeConfigInterface {
 public:
  LoadOobeConfigRollback(OobeConfig* oobe_config,
                         bool allow_unencrypted,
                         bool execute_commands);
  ~LoadOobeConfigRollback() = default;

  bool GetOobeConfigJson(std::string* config,
                         std::string* enrollment_domain) override;

 private:
  friend class LoadOobeConfigRollbackTest;
  FRIEND_TEST(LoadOobeConfigRollbackTest, CheckNotRollbackStages);
  FRIEND_TEST(LoadOobeConfigRollbackTest, CheckFirstStage);
  FRIEND_TEST(LoadOobeConfigRollbackTest, CheckSecondStage);
  FRIEND_TEST(LoadOobeConfigRollbackTest, CheckThirdStage);

  // There are 3 different stages for rollback data restore:
  // - 1st stage: rollback_data is present in kUnencryptedRollbackDataPath,
  //   first_stage_completed is not present. We have to extract the files from
  //   kUnencryptedRollbackDataPath and place them in kRestoreDestinationPath.
  // - 2nd stage: first_stage_completed is present, second_stage_completed is
  //   not present. oobe_config_finish_restore.sh takes care of this stage.
  // - 3rd stage: second_stage_completed is present. We restore data from
  //   kEncryptedRollbackDataPath, and remove everything indicating that a
  //   rollback happened.

  // Checks that we're in the first stage.
  bool CheckFirstStage();
  // Checks that we're in the second stage.
  bool CheckSecondStage();
  // Checks that we're in the third stage.
  bool CheckThirdStage();

  // Assembles a JSON config for Chrome based on rollback_data.
  bool AssembleConfig(const RollbackData& rollback_data, std::string* config);

  OobeConfig* oobe_config_;
  bool allow_unencrypted_ = false;
  bool execute_commands_ = true;

  DISALLOW_COPY_AND_ASSIGN(LoadOobeConfigRollback);
};

}  // namespace oobe_config

#endif  // OOBE_CONFIG_LOAD_OOBE_CONFIG_ROLLBACK_H_
