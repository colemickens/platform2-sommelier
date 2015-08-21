// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/base_api_handler.h"

#include <base/bind.h>

#include "libweave/src/commands/command_instance.h"
#include "libweave/src/commands/command_manager.h"
#include "libweave/src/device_registration_info.h"
#include "libweave/src/states/state_manager.h"

namespace weave {

namespace {
const char kBaseStateFirmwareVersion[] = "base.firmwareVersion";
const char kBaseStateAnonymousAccessRole[] = "base.localAnonymousAccessMaxRole";
const char kBaseStateDiscoveryEnabled[] = "base.localDiscoveryEnabled";
const char kBaseStatePairingEnabled[] = "base.localPairingEnabled";
}  // namespace

BaseApiHandler::BaseApiHandler(
    DeviceRegistrationInfo* device_info,
    const std::shared_ptr<StateManager>& state_manager,
    const std::shared_ptr<CommandManager>& command_manager)
    : device_info_{device_info}, state_manager_{state_manager} {
  device_info_->AddOnConfigChangedCallback(base::Bind(
      &BaseApiHandler::OnConfigChanged, weak_ptr_factory_.GetWeakPtr()));

  const Config& config = device_info_->GetConfig();
  base::DictionaryValue state;
  state.SetStringWithoutPathExpansion(kBaseStateFirmwareVersion,
                                      config.firmware_version());
  CHECK(state_manager_->SetProperties(state, nullptr));

  command_manager->AddOnCommandAddedCallback(base::Bind(
      &BaseApiHandler::OnCommandAdded, weak_ptr_factory_.GetWeakPtr()));
}

void BaseApiHandler::OnCommandAdded(Command* command) {
  if (command->GetStatus() != CommandStatus::kQueued)
    return;

  if (command->GetName() == "base.updateBaseConfiguration")
    return UpdateBaseConfiguration(command);

  if (command->GetName() == "base.updateDeviceInfo")
    return UpdateDeviceInfo(command);
}

void BaseApiHandler::UpdateBaseConfiguration(Command* command) {
  command->SetProgress(base::DictionaryValue{}, nullptr);

  const Config& config = device_info_->GetConfig();
  std::string anonymous_access_role{config.local_anonymous_access_role()};
  bool discovery_enabled{config.local_discovery_enabled()};
  bool pairing_enabled{config.local_pairing_enabled()};

  auto parameters = command->GetParameters();
  parameters->GetString("localAnonymousAccessMaxRole", &anonymous_access_role);
  parameters->GetBoolean("localDiscoveryEnabled", &discovery_enabled);
  parameters->GetBoolean("localPairingEnabled", &pairing_enabled);

  if (!device_info_->UpdateBaseConfig(anonymous_access_role, discovery_enabled,
                                      pairing_enabled, nullptr)) {
    return command->Abort();
  }

  command->Done();
}

void BaseApiHandler::OnConfigChanged(const Settings& settings) {
  base::DictionaryValue state;
  state.SetStringWithoutPathExpansion(kBaseStateAnonymousAccessRole,
                                      settings.local_anonymous_access_role);
  state.SetBooleanWithoutPathExpansion(kBaseStateDiscoveryEnabled,
                                       settings.local_discovery_enabled);
  state.SetBooleanWithoutPathExpansion(kBaseStatePairingEnabled,
                                       settings.local_pairing_enabled);
  state_manager_->SetProperties(state, nullptr);
}

void BaseApiHandler::UpdateDeviceInfo(Command* command) {
  command->SetProgress(base::DictionaryValue{}, nullptr);

  const Config& config = device_info_->GetConfig();
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
