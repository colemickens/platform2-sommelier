// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/commands/cloud_command_proxy.h"

#include "buffet/commands/command_instance.h"
#include "buffet/commands/prop_constraints.h"
#include "buffet/commands/prop_types.h"
#include "buffet/device_registration_info.h"

namespace buffet {

CloudCommandProxy::CloudCommandProxy(
    CommandInstance* command_instance,
    DeviceRegistrationInfo* device_registration_info)
    : command_instance_(command_instance),
      device_registration_info_(device_registration_info) {
}

void CloudCommandProxy::OnResultsChanged(const native_types::Object& results) {
  base::DictionaryValue patch;
  patch.Set("results", TypedValueToJson(results, nullptr).get());
  device_registration_info_->UpdateCommand(command_instance_->GetID(), patch);
}

void CloudCommandProxy::OnStatusChanged(const std::string& status) {
  base::DictionaryValue patch;
  // TODO(antonm): Change status to state.
  patch.SetString("state", status);
  device_registration_info_->UpdateCommand(command_instance_->GetID(), patch);
}

void CloudCommandProxy::OnProgressChanged(int progress) {
  base::DictionaryValue patch;
  patch.SetInteger("progress", progress);
  // TODO(antonm): Consider batching progress change updates.
  device_registration_info_->UpdateCommand(command_instance_->GetID(), patch);
}

}  // namespace buffet
