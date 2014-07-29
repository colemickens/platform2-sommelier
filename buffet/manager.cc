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
#include <dbus/bus.h>
#include <dbus/object_path.h>
#include <dbus/values_util.h>

#include "buffet/commands/command_manager.h"
#include "buffet/dbus_constants.h"
#include "buffet/dbus_utils.h"
#include "buffet/error.h"
#include "buffet/exported_object_manager.h"

using chromeos::dbus_utils::AsyncEventSequencer;
using buffet::dbus_utils::GetBadArgsError;
using buffet::dbus_utils::GetDBusError;

namespace buffet {

Manager::Manager(
    scoped_refptr<dbus::Bus> bus,
    base::WeakPtr<dbus_utils::ExportedObjectManager> object_manager)
    : bus_(bus),
      exported_object_(bus->GetExportedObject(
          dbus::ObjectPath(dbus_constants::kManagerServicePath))),
      object_manager_(object_manager) { }

Manager::~Manager() {
  object_manager_->ReleaseInterface(
      dbus::ObjectPath(dbus_constants::kManagerServicePath),
      dbus_constants::kManagerInterface);
  // Prevent the properties object from making calls to the exported object.
  properties_.reset(nullptr);
  // Unregister ourselves from the Bus.  This prevents the bus from calling
  // our callbacks in between the Manager's death and the bus unregistering
  // our exported object on shutdown.  Unretained makes no promises of memory
  // management.
  exported_object_->Unregister();
  exported_object_ = nullptr;
}

void Manager::Init(const OnInitFinish& cb) {
  scoped_refptr<AsyncEventSequencer> sequencer(
      new AsyncEventSequencer());
  exported_object_->ExportMethod(
      dbus_constants::kManagerInterface,
      dbus_constants::kManagerCheckDeviceRegistered,
      dbus_utils::GetExportableDBusMethod(
          base::Bind(&Manager::HandleCheckDeviceRegistered,
          base::Unretained(this))),
      sequencer->GetExportHandler(
          dbus_constants::kManagerInterface,
          dbus_constants::kManagerCheckDeviceRegistered,
          "Failed exporting CheckDeviceRegistered method",
          true));
  exported_object_->ExportMethod(
      dbus_constants::kManagerInterface,
      dbus_constants::kManagerGetDeviceInfo,
      dbus_utils::GetExportableDBusMethod(
          base::Bind(&Manager::HandleGetDeviceInfo,
          base::Unretained(this))),
      sequencer->GetExportHandler(
          dbus_constants::kManagerInterface,
          dbus_constants::kManagerGetDeviceInfo,
          "Failed exporting GetDeviceInfo method",
          true));
  exported_object_->ExportMethod(
      dbus_constants::kManagerInterface,
      dbus_constants::kManagerStartRegisterDevice,
      dbus_utils::GetExportableDBusMethod(
          base::Bind(&Manager::HandleStartRegisterDevice,
          base::Unretained(this))),
      sequencer->GetExportHandler(
          dbus_constants::kManagerInterface,
          dbus_constants::kManagerStartRegisterDevice,
          "Failed exporting StartRegisterDevice method",
          true));
  exported_object_->ExportMethod(
      dbus_constants::kManagerInterface,
      dbus_constants::kManagerFinishRegisterDevice,
      dbus_utils::GetExportableDBusMethod(
          base::Bind(&Manager::HandleFinishRegisterDevice,
          base::Unretained(this))),
      sequencer->GetExportHandler(
          dbus_constants::kManagerInterface,
          dbus_constants::kManagerFinishRegisterDevice,
          "Failed exporting FinishRegisterDevice method",
          true));
  exported_object_->ExportMethod(
      dbus_constants::kManagerInterface,
      dbus_constants::kManagerUpdateStateMethod,
      dbus_utils::GetExportableDBusMethod(
          base::Bind(&Manager::HandleUpdateState,
          base::Unretained(this))),
      sequencer->GetExportHandler(
          dbus_constants::kManagerInterface,
          dbus_constants::kManagerUpdateStateMethod,
          "Failed exporting UpdateState method",
          true));
  exported_object_->ExportMethod(
      dbus_constants::kManagerInterface, dbus_constants::kManagerTestMethod,
      dbus_utils::GetExportableDBusMethod(
          base::Bind(&Manager::HandleTestMethod, base::Unretained(this))),
      sequencer->GetExportHandler(
          dbus_constants::kManagerInterface, dbus_constants::kManagerTestMethod,
          "Failed exporting TestMethod method",
          true));
  properties_.reset(new Properties(bus_));
  // TODO(wiley): Initialize all properties appropriately before claiming
  //              the properties interface.
  properties_->state_.SetValue("{}");
  properties_->Init(
      sequencer->GetHandler("Manager properties export failed.", true));
  auto claim_interface_task = sequencer->WrapCompletionTask(
      base::Bind(&dbus_utils::ExportedObjectManager::ClaimInterface,
                 object_manager_->AsWeakPtr(),
                 dbus::ObjectPath(dbus_constants::kManagerServicePath),
                 dbus_constants::kManagerInterface,
                 properties_->GetPropertyWriter(
                     dbus_constants::kManagerInterface)));
  sequencer->OnAllTasksCompletedCall({claim_interface_task, cb});
  command_manager_ = std::make_shared<CommandManager>();
  command_manager_->Startup();
  device_info_ = std::unique_ptr<DeviceRegistrationInfo>(
      new DeviceRegistrationInfo(command_manager_));
  device_info_->Load();
}

scoped_ptr<dbus::Response> Manager::HandleCheckDeviceRegistered(
    dbus::MethodCall* method_call) {
  // Read the parameters to the method.
  dbus::MessageReader reader(method_call);
  if (reader.HasMoreData()) {
    return GetBadArgsError(method_call,
                           "Too many parameters to CheckDeviceRegistered");
  }

  LOG(INFO) << "Received call to Manager.CheckDeviceRegistered()";

  buffet::ErrorPtr error;
  bool registered = device_info_->CheckRegistration(&error);
  // If it fails due to any reason other than 'device not registered',
  // treat it as a real error and report it to the caller.
  if (!registered &&
      !error->HasError(kErrorDomainGCD, "device_not_registered")) {
    return GetDBusError(method_call, error.get());
  }

  std::string device_id;
  if (registered) {
    device_id = device_info_->GetDeviceId(&error);
    if (device_id.empty())
      return GetDBusError(method_call, error.get());
  }
  // Send back our response.
  scoped_ptr<dbus::Response> response(
      dbus::Response::FromMethodCall(method_call));
  dbus::MessageWriter writer(response.get());
  writer.AppendString(device_id);
  return response.Pass();
}

scoped_ptr<dbus::Response> Manager::HandleGetDeviceInfo(
    dbus::MethodCall* method_call) {
  // Read the parameters to the method.
  dbus::MessageReader reader(method_call);
  if (reader.HasMoreData()) {
    return GetBadArgsError(method_call,
                           "Too many parameters to GetDeviceInfo");
  }

  LOG(INFO) << "Received call to Manager.GetDeviceInfo()";

  std::string device_info_str;
  buffet::ErrorPtr error;
  auto device_info = device_info_->GetDeviceInfo(&error);
  if (!device_info)
    return GetDBusError(method_call, error.get());

  base::JSONWriter::Write(device_info.get(), &device_info_str);

  // Send back our response.
  scoped_ptr<dbus::Response> response(
      dbus::Response::FromMethodCall(method_call));
  dbus::MessageWriter writer(response.get());
  writer.AppendString(device_info_str);
  return response.Pass();
}

scoped_ptr<dbus::Response> Manager::HandleStartRegisterDevice(
    dbus::MethodCall* method_call) {
  // Read the parameters to the method.
  dbus::MessageReader reader(method_call);
  if (!reader.HasMoreData()) {
    return GetBadArgsError(method_call, "No parameters to StartRegisterDevice");
  }

  dbus::MessageReader array_reader(nullptr);
  if (!reader.PopArray(&array_reader))
    return GetBadArgsError(method_call, "Failed to read the parameter array");
  std::map<std::string, std::shared_ptr<base::Value>> params;
  while (array_reader.HasMoreData()) {
    dbus::MessageReader dict_entry_reader(nullptr);
    if (!array_reader.PopDictEntry(&dict_entry_reader))
      return GetBadArgsError(method_call, "Failed to get a call parameter");
    std::string key;
    if (!dict_entry_reader.PopString(&key))
      return GetBadArgsError(method_call, "Failed to read parameter key");
    base::Value* value = dbus::PopDataAsValue(&dict_entry_reader);
    if (!value)
      return GetBadArgsError(method_call, "Failed to read parameter value");
    params.insert(std::make_pair(key, std::shared_ptr<base::Value>(value)));
  }
  if (reader.HasMoreData())
    return GetBadArgsError(method_call,
                           "Too many parameters to StartRegisterDevice");

  LOG(INFO) << "Received call to Manager.StartRegisterDevice()";

  buffet::ErrorPtr error;
  std::string id = device_info_->StartRegistration(params, &error);
  if (id.empty())
    return GetDBusError(method_call, error.get());

  // Send back our response.
  scoped_ptr<dbus::Response> response(
      dbus::Response::FromMethodCall(method_call));
  dbus::MessageWriter writer(response.get());
  writer.AppendString(id);
  return response.Pass();
}

scoped_ptr<dbus::Response> Manager::HandleFinishRegisterDevice(
  dbus::MethodCall* method_call) {
  // Read the parameters to the method.
  dbus::MessageReader reader(method_call);
  if (!reader.HasMoreData()) {
    return GetBadArgsError(method_call,
                           "No parameters to FinishRegisterDevice");
  }
  std::string user_auth_code;
  if (!reader.PopString(&user_auth_code)) {
    return GetBadArgsError(method_call, "Failed to read UserAuthCode");
  }
  if (reader.HasMoreData()) {
    return GetBadArgsError(method_call,
                           "Too many parameters to FinishRegisterDevice");
  }

  LOG(INFO) << "Received call to Manager.FinishRegisterDevice()";
  buffet::ErrorPtr error;
  if (!device_info_->FinishRegistration(user_auth_code, &error))
    return GetDBusError(method_call, error.get());

  std::string device_id = device_info_->GetDeviceId(&error);
  if (device_id.empty())
    return GetDBusError(method_call, error.get());

  // Send back our response.
  scoped_ptr<dbus::Response> response(
    dbus::Response::FromMethodCall(method_call));
  dbus::MessageWriter writer(response.get());
  writer.AppendString(device_id);
  return response.Pass();
}

scoped_ptr<dbus::Response> Manager::HandleUpdateState(
    dbus::MethodCall *method_call) {
  // Read the parameters to the method.
  dbus::MessageReader reader(method_call);
  if (!reader.HasMoreData()) {
    return GetBadArgsError(method_call, "No parameters to UpdateState");
  }
  std::string json_state_fragment;
  if (!reader.PopString(&json_state_fragment)) {
    return GetBadArgsError(method_call, "Failed to read json_state_fragment");
  }
  if (reader.HasMoreData()) {
    return GetBadArgsError(method_call, "Too many parameters to UpdateState");
  }

  LOG(INFO) << "Received call to Manager.UpdateState()";
  // TODO(wiley): Merge json state blobs intelligently.
  properties_->state_.SetValue(json_state_fragment);

  // Send back our response.
  return dbus::Response::FromMethodCall(method_call);
}

scoped_ptr<dbus::Response> Manager::HandleTestMethod(
    dbus::MethodCall* method_call) {
  LOG(INFO) << "Received call to test method.";
  return scoped_ptr<dbus::Response>();
}

}  // namespace buffet
