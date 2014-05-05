// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_MANAGER_H_
#define BUFFET_MANAGER_H_

#include <base/basictypes.h>
#include <base/memory/scoped_ptr.h>
#include <base/values.h>
#include <dbus/message.h>
#include <dbus/object_path.h>
#include <memory>

#include "buffet/dbus_constants.h"
#include "buffet/exported_property_set.h"
#include "buffet/device_registration_info.h"

namespace buffet {

class DBusManager;

// The Manager is responsible for global state of Buffet.  It exposes
// interfaces which affect the entire device such as device registration and
// device state.
class Manager {
 public:
  typedef base::Callback<void(bool success)> OnInitFinish;

  Manager(dbus::Bus* bus);
  ~Manager();
  void Init(const OnInitFinish& cb);

 private:
  struct Properties: public dbus_utils::ExportedPropertySet {
   public:
    dbus_utils::ExportedProperty<std::string> state_;
    Properties(dbus::Bus* bus)
        : dbus_utils::ExportedPropertySet(
              bus, dbus::ObjectPath(dbus_constants::kManagerServicePath)) {
      RegisterProperty(dbus_constants::kManagerInterface, "State", &state_);
    }
    virtual ~Properties() {}
  };

  // Handles calls to org.chromium.Buffet.Manager.CheckDeviceRegistered().
  scoped_ptr<dbus::Response> HandleCheckDeviceRegistered(
      dbus::MethodCall* method_call);
  // Handles calls to org.chromium.Buffet.Manager.GetDeviceInfo().
  scoped_ptr<dbus::Response> HandleGetDeviceInfo(
      dbus::MethodCall* method_call);
  // Handles calls to org.chromium.Buffet.Manager.StartRegisterDevice().
  scoped_ptr<dbus::Response> HandleStartRegisterDevice(
      dbus::MethodCall* method_call);
  // Handles calls to org.chromium.Buffet.Manager.FinishRegisterDevice().
  scoped_ptr<dbus::Response> HandleFinishRegisterDevice(
      dbus::MethodCall* method_call);
  // Handles calls to org.chromium.Buffet.Manager.UpdateState().
  scoped_ptr<dbus::Response> HandleUpdateState(
      dbus::MethodCall* method_call);
  // Handles calls to org.chromium.Buffet.Manager.Test()
  scoped_ptr<::dbus::Response> HandleTestMethod(
      ::dbus::MethodCall* method_call);

  dbus::Bus* bus_;
  dbus::ExportedObject* exported_object_;  // weak; owned by the Bus object.
  scoped_ptr<Properties> properties_;

  DeviceRegistrationInfo device_info_;

  DISALLOW_COPY_AND_ASSIGN(Manager);
};

}  // namespace buffet

#endif  // BUFFET_MANAGER_H_
