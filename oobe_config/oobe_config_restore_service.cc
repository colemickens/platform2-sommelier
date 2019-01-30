// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "oobe_config/oobe_config_restore_service.h"

#include <string>

#include "oobe_config/load_oobe_config_rollback.h"
#include "oobe_config/load_oobe_config_usb.h"
#include "oobe_config/oobe_config.h"
#include "oobe_config/proto_bindings/oobe_config.pb.h"

using brillo::dbus_utils::AsyncEventSequencer;

namespace oobe_config {

OobeConfigRestoreService::OobeConfigRestoreService(
    std::unique_ptr<brillo::dbus_utils::DBusObject> dbus_object,
    bool allow_unencrypted,
    bool skip_reboot_for_testing)
    : org::chromium::OobeConfigRestoreAdaptor(this),
      dbus_object_(std::move(dbus_object)),
      allow_unencrypted_(allow_unencrypted),
      skip_reboot_for_testing_(skip_reboot_for_testing) {}

OobeConfigRestoreService::~OobeConfigRestoreService() = default;

void OobeConfigRestoreService::RegisterAsync(
    const AsyncEventSequencer::CompletionAction& completion_callback) {
  RegisterWithDBusObject(dbus_object_.get());
  dbus_object_->RegisterAsync(completion_callback);
}

void OobeConfigRestoreService::ProcessAndGetOobeAutoConfig(
    int32_t* error, OobeRestoreData* oobe_config_proto) {
  DCHECK(error);
  DCHECK(oobe_config_proto);

  LOG(INFO) << "Chrome requested OOBE config.";
  power_manager_proxy_ = std::make_unique<org::chromium::PowerManagerProxy>(
      dbus_object_->GetBus());

  OobeConfig oobe_config;
  LoadOobeConfigRollback load_oobe_config_rollback(
      &oobe_config, allow_unencrypted_, skip_reboot_for_testing_,
      power_manager_proxy_.get());
  std::string chrome_config_json, unused_enrollment_domain;

  // There is rollback data so attempt to parse it.
  const bool rollback_success = load_oobe_config_rollback.GetOobeConfigJson(
      &chrome_config_json, &unused_enrollment_domain);
  if (rollback_success) {
    LOG(WARNING) << "Rollback oobe config sent: " << chrome_config_json;
  } else {
    LOG(INFO) << "Rollback oobe config not found.";

    // There is no rollback data so attempt USB config.
    auto config_loader = LoadOobeConfigUsb::CreateInstance();

    const bool usb_config_success = config_loader->GetOobeConfigJson(
        &chrome_config_json, &unused_enrollment_domain);
    if (usb_config_success) {
      LOG(INFO) << "USB oobe config found :) ";
    } else {
      LOG(WARNING) << "USB oobe config not found :(";
    }
  }

  oobe_config_proto->set_chrome_config_json(chrome_config_json);
  *error = 0;
}

}  // namespace oobe_config
