// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/manager.h"

#include <base/bind.h>
#include <base/bind_helpers.h>

#include "buffet/dbus_constants.h"
#include "buffet/dbus_manager.h"
#include "buffet/dbus_utils.h"

using buffet::dbus_utils::GetBadArgsError;

namespace buffet {

Manager::Manager(DBusManager* dbus_manager) : dbus_manager_(dbus_manager) {
  dbus::ExportedObject* exported_object = dbus_manager_->GetExportedObject(
      dbus_constants::kManagerServicePath);
  dbus_manager_->ExportDBusMethod(exported_object,
                                  dbus_constants::kManagerInterface,
                                  dbus_constants::kManagerRegisterDeviceMethod,
                                  base::Bind(&Manager::HandleRegisterDevice,
                                             base::Unretained(this)));
  dbus_manager_->ExportDBusMethod(exported_object,
                                  dbus_constants::kManagerInterface,
                                  dbus_constants::kManagerUpdateStateMethod,
                                  base::Bind(&Manager::HandleUpdateState,
                                             base::Unretained(this)));
  properties_.reset(new Properties(exported_object));
  // TODO(wiley): Initialize all properties appropriately before claiming
  //              the properties interface.
  properties_->state_.SetValue("{}");
  properties_->ClaimPropertiesInterface();
}

Manager::~Manager() {
  // Prevent the properties object from making calls to the exported object.
  properties_.reset(nullptr);
  // Unregister ourselves from the Bus.  This prevents the bus from calling
  // our callbacks in between the Manager's death and the bus unregistering
  // our exported object on shutdown.  Unretained makes no promises of memory
  // management.
  auto exported_object = dbus_manager_->GetExportedObject(
      dbus_constants::kManagerServicePath);
  exported_object->Unregister();
}

scoped_ptr<dbus::Response> Manager::HandleRegisterDevice(
    dbus::MethodCall* method_call) {
  // Read the parameters to the method.
  dbus::MessageReader reader(method_call);
  if (!reader.HasMoreData()) {
    return GetBadArgsError(method_call, "No parameters to RegisterDevice");
  }
  std::string client_id, client_secret, api_key;
  if (!reader.PopString(&client_id)) {
    return GetBadArgsError(method_call, "Failed to read client_id");
  }
  if (!reader.PopString(&client_secret)) {
    return GetBadArgsError(method_call, "Failed to read client_secret");
  }
  if (!reader.PopString(&api_key)) {
    return GetBadArgsError(method_call, "Failed to read api_key");
  }
  if (reader.HasMoreData()) {
    return GetBadArgsError(
        method_call, "Too many parameters to RegisterDevice");
  }

  LOG(INFO) << "Received call to Manager.RegisterDevice()";
  // TODO(wiley): Do something with these parameters to register the device.

  // Send back our response.
  scoped_ptr<dbus::Response> response(
      dbus::Response::FromMethodCall(method_call));
  dbus::MessageWriter writer(response.get());
  writer.AppendString("<registration ticket id>");
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

}  // namespace buffet
