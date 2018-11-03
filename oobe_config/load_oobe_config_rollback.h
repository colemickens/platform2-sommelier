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
#include <power_manager-client/power_manager/dbus-proxies.h>

namespace oobe_config {

class OobeConfig;
class RollbackData;

// An object of this class has the responsibility of loading the oobe config
// file after rollback.
class LoadOobeConfigRollback : public LoadOobeConfigInterface {
 public:
  LoadOobeConfigRollback(OobeConfig* oobe_config,
                         bool allow_unencrypted,
                         org::chromium::PowerManagerProxy* power_manager_proxy);
  ~LoadOobeConfigRollback() = default;

  bool GetOobeConfigJson(std::string* config,
                         std::string* enrollment_domain) override;

 private:
  // Assembles a JSON config for Chrome based on rollback_data.
  bool AssembleConfig(const RollbackData& rollback_data, std::string* config);

  OobeConfig* oobe_config_;
  bool allow_unencrypted_ = false;
  org::chromium::PowerManagerProxy* power_manager_proxy_;

  DISALLOW_COPY_AND_ASSIGN(LoadOobeConfigRollback);
};

}  // namespace oobe_config

#endif  // OOBE_CONFIG_LOAD_OOBE_CONFIG_ROLLBACK_H_
