// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/base_api_handler.h"

#include "libweave/src/commands/command_instance.h"
#include "libweave/src/commands/command_manager.h"
#include "libweave/src/device_registration_info.h"
#include "libweave/src/states/state_manager.h"

namespace weave {

BaseApiHandler::BaseApiHandler(
    const base::WeakPtr<DeviceRegistrationInfo>& device_info,
    const std::shared_ptr<StateManager>& state_manager,
    const std::shared_ptr<CommandManager>& command_manager)
    : device_info_{device_info}, state_manager_{state_manager} {
  command_manager->AddOnCommandAddedCallback(base::Bind(
      &BaseApiHandler::OnCommandAdded, weak_ptr_factory_.GetWeakPtr()));
}

void BaseApiHandler::OnCommandAdded(Command* command) {
  if (command->GetStatus() != CommandInstance::kStatusQueued)
    return;

  if (command->GetName() == "base.updateBaseConfiguration")
    return UpdateBaseConfiguration(command);

  if (command->GetName() == "base.updateDeviceInfo")
    return UpdateDeviceInfo(command);
}

void BaseApiHandler::UpdateBaseConfiguration(Command* command) {
  command->SetProgress(base::DictionaryValue{}, nullptr);

  const BuffetConfig& config{device_info_->GetConfig()};
  std::string anonymous_access_role{config.local_anonymous_access_role()};
  bool discovery_enabled{config.local_discovery_enabled()};
  bool pairing_enabled{config.local_pairing_enabled()};

  auto parameters = command->GetParameters();
  parameters->GetString("localAnonymousAccessMaxRole", &anonymous_access_role);
  parameters->GetBoolean("localDiscoveryEnabled", &discovery_enabled);
  parameters->GetBoolean("localPairingEnabled", &pairing_enabled);

  chromeos::VariantDictionary state{
      {"base.localAnonymousAccessMaxRole", anonymous_access_role},
      {"base.localDiscoveryEnabled", discovery_enabled},
      {"base.localPairingEnabled", pairing_enabled},
  };
  if (!state_manager_->SetProperties(state, nullptr)) {
    return command->Abort();
  }

  if (!device_info_->UpdateBaseConfig(anonymous_access_role, discovery_enabled,
                                      pairing_enabled, nullptr)) {
    return command->Abort();
  }

  command->Done();
}

void BaseApiHandler::UpdateDeviceInfo(Command* command) {
  command->SetProgress(base::DictionaryValue{}, nullptr);

  const BuffetConfig& config{device_info_->GetConfig()};
  std::string name{config.name()};
  std::string description{config.description()};
  std::string location{config.location()};

  auto parameters = command->GetParameters();
  parameters->GetString("name", &name);
  parameters->GetString("description", &description);
  parameters->GetString("location", &location);

  if (!device_info_->UpdateDeviceInfo(name, description, location, nullptr)) {
    return command->Abort();
  }

  command->Done();
}

}  // namespace weave
