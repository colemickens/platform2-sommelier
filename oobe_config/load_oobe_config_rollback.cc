// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "oobe_config/load_oobe_config_rollback.h"

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/json/json_writer.h>
#include <base/values.h>
#include <power_manager-client/power_manager/dbus-constants.h>

#include "oobe_config/oobe_config.h"
#include "oobe_config/rollback_constants.h"
#include "oobe_config/rollback_data.pb.h"

using base::FilePath;
using std::string;
using std::unique_ptr;

namespace oobe_config {

LoadOobeConfigRollback::LoadOobeConfigRollback(
    OobeConfig* oobe_config,
    bool allow_unencrypted,
    org::chromium::PowerManagerProxy* power_manager_proxy)
    : oobe_config_(oobe_config),
      allow_unencrypted_(allow_unencrypted),
      power_manager_proxy_(power_manager_proxy) {}

bool LoadOobeConfigRollback::CheckFirstStage() {
  // Check whether we're in the first stage.
  if (!oobe_config_->FileExists(kUnencryptedStatefulRollbackDataPath)) {
    LOG(INFO) << "Rollback data "
              << kUnencryptedStatefulRollbackDataPath.value()
              << " does not exist.";
    return false;
  }
  if (oobe_config_->FileExists(kFirstStageCompletedFile)) {
    LOG(INFO) << "First stage already completed.";
    return false;
  }

  // At this point, we should be in the first stage. We verify, that the other
  // files are in a consistent state.
  if (oobe_config_->FileExists(kSecondStageCompletedFile)) {
    LOG(ERROR) << "Second stage is completed but first stage is not.";
    return false;
  }
  if (oobe_config_->FileExists(kEncryptedStatefulRollbackDataPath)) {
    LOG(ERROR) << "Both encrypted and unencrypted rollback data path exists.";
    return false;
  }

  // Everything is awesome.
  return true;
}

bool LoadOobeConfigRollback::CheckSecondStage() {
  // Check whether we're in the second stage.
  if (!oobe_config_->FileExists(kFirstStageCompletedFile)) {
    LOG(INFO) << "First stage not yet completed.";
    return false;
  }
  if (oobe_config_->FileExists(kSecondStageCompletedFile)) {
    LOG(INFO) << "Second stage already completed.";
    return false;
  }

  // At this point, we should be in the second stage. We verify, that the other
  // files are in a consistent state.
  if (!oobe_config_->FileExists(kUnencryptedStatefulRollbackDataPath)) {
    LOG(ERROR) << "Rollback data "
               << kUnencryptedStatefulRollbackDataPath.value()
               << " should exist in second stage.";
    return false;
  }
  if (!oobe_config_->FileExists(kEncryptedStatefulRollbackDataPath)) {
    LOG(ERROR) << "Rollback data " << kEncryptedStatefulRollbackDataPath.value()
               << " should exist in second stage.";
    return false;
  }
  return true;
}

bool LoadOobeConfigRollback::CheckThirdStage() {
  if (!oobe_config_->FileExists(kSecondStageCompletedFile)) {
    LOG(INFO) << "Second stage not yet completed.";
    return false;
  }

  // At this point, we should be in the third stage. We verify, that the other
  // files are in a consistent state.
  if (!oobe_config_->FileExists(kFirstStageCompletedFile)) {
    LOG(ERROR) << "First stage should be already completed.";
    return false;
  }
  if (oobe_config_->FileExists(kUnencryptedStatefulRollbackDataPath)) {
    LOG(ERROR) << "Rollback data "
               << kUnencryptedStatefulRollbackDataPath.value()
               << " should not exist in third stage.";
    return false;
  }
  if (!oobe_config_->FileExists(kEncryptedStatefulRollbackDataPath)) {
    LOG(ERROR) << "Rollback data " << kEncryptedStatefulRollbackDataPath.value()
               << " should exist in third stage.";
    return false;
  }

  return true;
}

bool LoadOobeConfigRollback::GetOobeConfigJson(string* config,
                                               string* enrollment_domain) {
  *config = "";
  *enrollment_domain = "";

  // Precondition for running rollback.
  if (!oobe_config_->FileExists(kRestoreTempPath)) {
    LOG(ERROR) << "Restore destination path doesn't exist.";
    return false;
  }

  if (CheckFirstStage()) {
    OobeConfig oobe_config;
    // In the first stage we decrypt the proto from kUnencryptedRollbackDataPath
    // and save it unencrypted to kEncryptedStatefulRollbackDataPath.
    if (allow_unencrypted_) {
      oobe_config.UnencryptedRollbackRestore();
    } else {
      // TODO(zentaro): Add encrypted rollback restore.
      LOG(ERROR) << "Encrypted rollback is not yet supported.";
      return false;
    }

    // We create kFirstStageCompletedFile after this.
    oobe_config.WriteFile(kFirstStageCompletedFile, "");
    // If all succeeded, we reboot.
    if (base::PathExists(kFirstStageCompletedFile) && power_manager_proxy_) {
      LOG(INFO) << "Rebooting device.";
      brillo::ErrorPtr error;
      if (!power_manager_proxy_->RequestRestart(
              ::power_manager::REQUEST_RESTART_OTHER,
              "oobe_config: reboot after rollback restore first stage",
              &error)) {
        LOG(ERROR) << "Failed to reboot device, error: " << error->GetMessage();
      }
    }
    return false;
  }

  if (CheckSecondStage()) {
    // This shouldn't happen, the script failed to execute. We fail and return
    // false.
    LOG(ERROR) << "Rollback restore is in invalid state (stage 2).";
    return false;
  }

  if (CheckThirdStage()) {
    OobeConfig oobe_config;
    // We load the proto from kEncryptedStatefulRollbackDataPath.
    string rollback_data_str;
    if (!oobe_config.ReadFile(kEncryptedStatefulRollbackDataPath,
                              &rollback_data_str)) {
      return false;
    }
    RollbackData rollback_data;
    if (!rollback_data.ParseFromString(rollback_data_str)) {
      LOG(ERROR) << "Couldn't parse proto.";
      return false;
    }
    // We get the data for Chrome and assemble the config.
    if (!AssembleConfig(rollback_data, config)) {
      LOG(ERROR) << "Failed to assemble config.";
      return false;
    }
    // If it succeeded, we remove all files from
    // kEncryptedStatefulRollbackDataPath.
    oobe_config.CleanupEncryptedStatefulDirectory();
    return true;
  }
  // We are not in any legitimate rollback stage.
  return false;
}

bool LoadOobeConfigRollback::AssembleConfig(const RollbackData& rollback_data,
                                            string* config) {
  // Possible values are defined in
  // chrome/browser/resources/chromeos/login/oobe_types.js.
  // TODO(zentaro): Export these strings as constants.
  base::DictionaryValue dictionary;
  // Always skip next screen.
  dictionary.SetBoolean("welcomeNext", true);
  // Always skip network selection screen if possible.
  dictionary.SetBoolean("networkUseConnected", true);
  // We don't want updates after rolling back.
  dictionary.SetBoolean("updateSkipNonCritical", true);
  // TODO(zentaro): Set this through protobuf.
  dictionary.SetBoolean("eulaAutoAccept", true);
  dictionary.SetBoolean("eulaSendStatistics", false);
  return base::JSONWriter::Write(dictionary, config);
}

}  // namespace oobe_config
