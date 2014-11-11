// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/manager.h"

#include <map>
#include <string>

#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/json/json_reader.h>
#include <base/json/json_writer.h>
#include <chromeos/dbus/async_event_sequencer.h>
#include <chromeos/dbus/exported_object_manager.h>
#include <chromeos/errors/error.h>
#include <dbus/bus.h>
#include <dbus/object_path.h>
#include <dbus/values_util.h>

#include "buffet/commands/command_instance.h"
#include "buffet/commands/command_manager.h"
#include "buffet/libbuffet/dbus_constants.h"
#include "buffet/states/state_change_queue.h"
#include "buffet/states/state_manager.h"

using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::dbus_utils::DBusMethodResponse;
using chromeos::dbus_utils::ExportedObjectManager;

namespace buffet {

namespace {
// Max of 100 state update events should be enough in the queue.
const size_t kMaxStateChangeQueueSize = 100;
}  // anonymous namespace

Manager::Manager(const base::WeakPtr<ExportedObjectManager>& object_manager)
    : dbus_object_(object_manager.get(),
                   object_manager->GetBus(),
                   dbus::ObjectPath(dbus_constants::kManagerServicePath)) {}

Manager::~Manager() {}

void Manager::RegisterAsync(const AsyncEventSequencer::CompletionAction& cb) {
  chromeos::dbus_utils::DBusInterface* itf =
      dbus_object_.AddOrGetInterface(dbus_constants::kManagerInterface);
  itf->AddMethodHandler(dbus_constants::kManagerStartDevice,
                        base::Unretained(this),
                        &Manager::HandleStartDevice);
  itf->AddMethodHandler(dbus_constants::kManagerCheckDeviceRegistered,
                        base::Unretained(this),
                        &Manager::HandleCheckDeviceRegistered);
  itf->AddMethodHandler(dbus_constants::kManagerGetDeviceInfo,
                        base::Unretained(this),
                        &Manager::HandleGetDeviceInfo);
  itf->AddMethodHandler(dbus_constants::kManagerRegisterDevice,
                        base::Unretained(this),
                        &Manager::HandleRegisterDevice);
  itf->AddMethodHandler(dbus_constants::kManagerUpdateStateMethod,
                        base::Unretained(this),
                        &Manager::HandleUpdateState);
  itf->AddMethodHandler(dbus_constants::kManagerAddCommand,
                        base::Unretained(this),
                        &Manager::HandleAddCommand);
  itf->AddSimpleMethodHandler(dbus_constants::kManagerTestMethod,
                              base::Unretained(this),
                              &Manager::HandleTestMethod);
  dbus_object_.RegisterAsync(cb);
  command_manager_ =
      std::make_shared<CommandManager>(dbus_object_.GetObjectManager());
  command_manager_->Startup();
  state_change_queue_ = std::unique_ptr<StateChangeQueue>(
      new StateChangeQueue(kMaxStateChangeQueueSize));
  state_manager_ = std::make_shared<StateManager>(state_change_queue_.get());
  state_manager_->Startup();
  device_info_ = std::unique_ptr<DeviceRegistrationInfo>(
      new DeviceRegistrationInfo(command_manager_, state_manager_));
  device_info_->Load();
}

void Manager::HandleStartDevice(scoped_ptr<DBusMethodResponse> response) {
  LOG(INFO) << "Received call to Manager.StartDevice()";

  chromeos::ErrorPtr error;
  device_info_->StartDevice(&error);
  if (error)
    response->ReplyWithError(error.get());
  else
    response->Return();
}

void Manager::HandleCheckDeviceRegistered(
    scoped_ptr<DBusMethodResponse> response) {
  LOG(INFO) << "Received call to Manager.CheckDeviceRegistered()";
  chromeos::ErrorPtr error;
  bool registered = device_info_->CheckRegistration(&error);
  // If it fails due to any reason other than 'device not registered',
  // treat it as a real error and report it to the caller.
  if (!registered &&
      !error->HasError(kErrorDomainGCD, "device_not_registered")) {
    response->ReplyWithError(error.get());
    return;
  }

  std::string device_id;
  if (registered) {
    error.reset();
    device_id = device_info_->GetDeviceId(&error);
    if (error) {
      response->ReplyWithError(error.get());
      return;
    }
  }

  response->Return(device_id);
}

void Manager::HandleGetDeviceInfo(
    scoped_ptr<DBusMethodResponse> response) {
  LOG(INFO) << "Received call to Manager.GetDeviceInfo()";

  chromeos::ErrorPtr error;
  auto device_info = device_info_->GetDeviceInfo(&error);
  if (!device_info) {
    response->ReplyWithError(error.get());
    return;
  }

  std::string device_info_str;
  base::JSONWriter::Write(device_info.get(), &device_info_str);
  response->Return(device_info_str);
}

void Manager::HandleRegisterDevice(
    scoped_ptr<DBusMethodResponse> response,
    const std::map<std::string, std::string>& params) {
  LOG(INFO) << "Received call to Manager.RegisterDevice()";

  chromeos::ErrorPtr error;
  std::string device_id = device_info_->RegisterDevice(params, &error);
  if (error)
    response->ReplyWithError(error.get());
  else
    response->Return(device_id);
}

void Manager::HandleUpdateState(
    scoped_ptr<DBusMethodResponse> response,
    const chromeos::VariantDictionary& property_set) {
  chromeos::ErrorPtr error;
  base::Time timestamp = base::Time::Now();
  bool all_success = true;
  for (const auto& pair : property_set) {
    if (!state_manager_->SetPropertyValue(pair.first, pair.second,
                                          timestamp, &error)) {
      // Remember that an error occurred but keep going and update the rest of
      // the properties if possible.
      all_success = false;
    }
  }
  if (!all_success)
    response->ReplyWithError(error.get());
  else
    response->Return();
}

void Manager::HandleAddCommand(scoped_ptr<DBusMethodResponse> response,
                               const std::string& json_command) {
  static int next_id = 0;
  std::string error_message;
  std::unique_ptr<base::Value> value(base::JSONReader::ReadAndReturnError(
      json_command, base::JSON_PARSE_RFC, nullptr, &error_message));
  if (!value) {
    response->ReplyWithError(FROM_HERE, chromeos::errors::json::kDomain,
                             chromeos::errors::json::kParseError,
                             error_message);
    return;
  }
  chromeos::ErrorPtr error;
  auto command_instance = buffet::CommandInstance::FromJson(
      value.get(), command_manager_->GetCommandDictionary(), &error);
  if (!command_instance) {
    response->ReplyWithError(error.get());
    return;
  }
  command_instance->SetID(std::to_string(++next_id));
  command_manager_->AddCommand(std::move(command_instance));
  response->Return();
}

std::string Manager::HandleTestMethod(const std::string& message) {
  LOG(INFO) << "Received call to test method: " << message;
  return message;
}

}  // namespace buffet
