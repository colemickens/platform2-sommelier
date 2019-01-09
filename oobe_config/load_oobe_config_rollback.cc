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
    bool skip_reboot_for_testing,
    org::chromium::PowerManagerProxy* power_manager_proxy)
    : oobe_config_(oobe_config),
      allow_unencrypted_(allow_unencrypted),
      skip_reboot_for_testing_(skip_reboot_for_testing),
      power_manager_proxy_(power_manager_proxy) {}

bool LoadOobeConfigRollback::GetOobeConfigJson(string* config,
                                               string* enrollment_domain) {
  LOG(INFO) << "Looking for rollback state.";

  *config = "";
  *enrollment_domain = "";

  // Precondition for running rollback.
  if (!oobe_config_->FileExists(kRestoreTempPath)) {
    LOG(ERROR) << "Restore destination path doesn't exist.";
    return false;
  }

  if (oobe_config_->CheckFirstStage()) {
    LOG(INFO) << "Starting rollback restore stage 1.";

    // In the first stage we decrypt the proto from kUnencryptedRollbackDataPath
    // and save it unencrypted to kEncryptedStatefulRollbackDataPath.
    if (allow_unencrypted_) {
      oobe_config_->UnencryptedRollbackRestore();
    } else {
      oobe_config_->EncryptedRollbackRestore();
    }

    // We create kFirstStageCompletedFile after this.
    oobe_config_->WriteFile(kFirstStageCompletedFile, "");
    // If all succeeded, we reboot.
    if (base::PathExists(kFirstStageCompletedFile) && power_manager_proxy_) {
      if (!skip_reboot_for_testing_) {
        LOG(INFO) << "Rebooting device.";
        brillo::ErrorPtr error;
        if (!power_manager_proxy_->RequestRestart(
                ::power_manager::REQUEST_RESTART_OTHER,
                "oobe_config: reboot after rollback restore first stage",
                &error)) {
          LOG(ERROR) << "Failed to reboot device, error: "
                     << error->GetMessage();
        }
      } else {
        LOG(INFO) << "Skipping reboot for testing";
      }
    }
    return false;
  }

  if (oobe_config_->CheckSecondStage()) {
    // This shouldn't happen, the script failed to execute. We fail and return
    // false.
    LOG(ERROR) << "Rollback restore is in invalid state (stage 2).";
    return false;
  }

  if (oobe_config_->CheckThirdStage()) {
    LOG(INFO) << "Starting rollback restore stage 3.";

    // We load the proto from kEncryptedStatefulRollbackDataPath.
    string rollback_data_str;
    if (!oobe_config_->ReadFile(kEncryptedStatefulRollbackDataPath,
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
    LOG(INFO) << "Cleaning up rollback data.";
    oobe_config_->CleanupEncryptedStatefulDirectory();

    LOG(INFO) << "Rollback restore completed successfully.";
    return true;
  }

  // We are not in any legitimate rollback stage.
  LOG(ERROR) << "Rollback is in an invalid state.";
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
  // Set whether metrics should be enabled if it exists in |rollback_data|.
  dictionary.SetBoolean("eulaSendStatistics",
                        rollback_data.eula_send_statistics());
  // TODO(zentaro): Set this through protobuf.
  dictionary.SetBoolean("eulaAutoAccept", true);

  return base::JSONWriter::Write(dictionary, config);
}

}  // namespace oobe_config
