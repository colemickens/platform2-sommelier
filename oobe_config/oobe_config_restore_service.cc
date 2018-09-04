// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "oobe_config/oobe_config_restore_service.h"

using brillo::dbus_utils::AsyncEventSequencer;

namespace oobe_config {

OobeConfigRestoreService::OobeConfigRestoreService(
    std::unique_ptr<brillo::dbus_utils::DBusObject> dbus_object)
    : org::chromium::OobeConfigRestoreAdaptor(this),
      dbus_object_(std::move(dbus_object)) {}

OobeConfigRestoreService::~OobeConfigRestoreService() = default;

void OobeConfigRestoreService::RegisterAsync(
    const AsyncEventSequencer::CompletionAction& completion_callback) {
  RegisterWithDBusObject(dbus_object_.get());
  dbus_object_->RegisterAsync(completion_callback);
}

void OobeConfigRestoreService::ProcessAndGetOobeAutoConfig(
    int32_t* error, std::vector<uint8_t>* oobe_config_blob) {
  DCHECK(error);
  DCHECK(oobe_config_blob);

  LOG(INFO) << "Called ProcessAndGetOobeAutoConfig";
  *error = 0;
}

}  // namespace oobe_config
