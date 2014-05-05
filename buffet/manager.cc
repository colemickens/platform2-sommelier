// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/manager.h"

#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/json/json_writer.h>
#include <dbus/bus.h>
#include <dbus/object_path.h>
#include <dbus/values_util.h>

#include "buffet/async_event_sequencer.h"
#include "buffet/dbus_constants.h"
#include "buffet/dbus_utils.h"

using buffet::dbus_utils::GetBadArgsError;

namespace buffet {

Manager::Manager(dbus::Bus* bus)
    : bus_(bus),
      exported_object_(bus->GetExportedObject(
          dbus::ObjectPath(dbus_constants::kManagerServicePath))) { }

Manager::~Manager() {
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
  scoped_refptr<dbus_utils::AsyncEventSequencer> sequencer(
      new dbus_utils::AsyncEventSequencer());
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
  sequencer->OnAllTasksCompletedCall({cb});
  device_info_.Load();
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

  bool registered = device_info_.CheckRegistration();

  // Send back our response.
  scoped_ptr<dbus::Response> response(
      dbus::Response::FromMethodCall(method_call));
  dbus::MessageWriter writer(response.get());
  writer.AppendString(registered ? device_info_.GetDeviceId() : std::string());
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
  std::unique_ptr<base::Value> device_info = device_info_.GetDeviceInfo();
  if (device_info)
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

  std::string error_msg;
  std::string id = device_info_.StartRegistration(params, &error_msg);
  if(id.empty())
    return GetBadArgsError(method_call, error_msg);

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
  bool success = device_info_.FinishRegistration(user_auth_code);

  // Send back our response.
  scoped_ptr<dbus::Response> response(
    dbus::Response::FromMethodCall(method_call));
  dbus::MessageWriter writer(response.get());
  writer.AppendString(success ? device_info_.GetDeviceId() : std::string());
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
