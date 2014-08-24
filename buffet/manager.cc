// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/manager.h"

#include <map>
#include <string>

#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/json/json_writer.h>
#include <chromeos/async_event_sequencer.h>
#include <chromeos/dbus_utils.h>
#include <chromeos/error.h>
#include <chromeos/exported_object_manager.h>
#include <dbus/bus.h>
#include <dbus/object_path.h>
#include <dbus/values_util.h>

#include "buffet/commands/command_manager.h"
#include "buffet/dbus_constants.h"

using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::dbus_utils::ExportedObjectManager;

namespace buffet {

Manager::Manager(const base::WeakPtr<ExportedObjectManager>& object_manager)
    : dbus_object_(object_manager.get(),
                   object_manager->GetBus(),
                   dbus::ObjectPath(dbus_constants::kManagerServicePath)) {}

void Manager::RegisterAsync(const AsyncEventSequencer::CompletionAction& cb) {
  chromeos::dbus_utils::DBusInterface* itf =
      dbus_object_.AddOrGetInterface(dbus_constants::kManagerInterface);
  itf->AddMethodHandler(dbus_constants::kManagerCheckDeviceRegistered,
                        base::Unretained(this),
                        &Manager::HandleCheckDeviceRegistered);
  itf->AddMethodHandler(dbus_constants::kManagerGetDeviceInfo,
                        base::Unretained(this),
                        &Manager::HandleGetDeviceInfo);
  itf->AddMethodHandler(dbus_constants::kManagerStartRegisterDevice,
                        base::Unretained(this),
                        &Manager::HandleStartRegisterDevice);
  itf->AddMethodHandler(dbus_constants::kManagerFinishRegisterDevice,
                        base::Unretained(this),
                        &Manager::HandleFinishRegisterDevice);
  itf->AddMethodHandler(dbus_constants::kManagerUpdateStateMethod,
                        base::Unretained(this),
                        &Manager::HandleUpdateState);
  itf->AddMethodHandler(dbus_constants::kManagerTestMethod,
                        base::Unretained(this),
                        &Manager::HandleTestMethod);
  itf->AddProperty("State", &state_);
  // TODO(wiley): Initialize all properties appropriately before claiming
  //              the properties interface.
  state_.SetValue("{}");
  dbus_object_.RegisterAsync(cb);
  command_manager_ = std::make_shared<CommandManager>();
  command_manager_->Startup();
  device_info_ = std::unique_ptr<DeviceRegistrationInfo>(
      new DeviceRegistrationInfo(command_manager_));
  device_info_->Load();
}

std::string Manager::HandleCheckDeviceRegistered(chromeos::ErrorPtr* error) {
  LOG(INFO) << "Received call to Manager.CheckDeviceRegistered()";
  std::string device_id;
  bool registered = device_info_->CheckRegistration(error);
  // If it fails due to any reason other than 'device not registered',
  // treat it as a real error and report it to the caller.
  if (!registered &&
      !(*error)->HasError(kErrorDomainGCD, "device_not_registered")) {
    return device_id;
  }

  error->reset();

  if (registered)
    device_id = device_info_->GetDeviceId(error);

  return device_id;
}

std::string Manager::HandleGetDeviceInfo(chromeos::ErrorPtr* error) {
  LOG(INFO) << "Received call to Manager.GetDeviceInfo()";

  std::string device_info_str;
  auto device_info = device_info_->GetDeviceInfo(error);
  if (!device_info)
    return device_info_str;

  base::JSONWriter::Write(device_info.get(), &device_info_str);
  return device_info_str;
}

std::string Manager::HandleStartRegisterDevice(
    chromeos::ErrorPtr* error,
    const std::map<std::string, std::string>& params) {
  LOG(INFO) << "Received call to Manager.StartRegisterDevice()";

  return device_info_->StartRegistration(params, error);
}

std::string Manager::HandleFinishRegisterDevice(
    chromeos::ErrorPtr* error, const std::string& user_auth_code) {
  LOG(INFO) << "Received call to Manager.FinishRegisterDevice()";
  if (!device_info_->FinishRegistration(user_auth_code, error))
    return std::string();

  return device_info_->GetDeviceId(error);
}

void Manager::HandleUpdateState(
    chromeos::ErrorPtr* error, const std::string& json_state_fragment) {
  // TODO(wiley): Merge json state blobs intelligently.
  state_.SetValue(json_state_fragment);
}

std::string Manager::HandleTestMethod(chromeos::ErrorPtr* error,
                                      const std::string& message) {
  LOG(INFO) << "Received call to test method: " << message;
  return message;
}

}  // namespace buffet
