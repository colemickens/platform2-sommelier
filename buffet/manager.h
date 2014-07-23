// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_MANAGER_H_
#define BUFFET_MANAGER_H_

#include <memory>
#include <string>

#include <base/basictypes.h>
#include <base/memory/scoped_ptr.h>
#include <base/memory/weak_ptr.h>
#include <base/values.h>
#include <dbus/message.h>
#include <dbus/object_path.h>

#include "buffet/dbus_constants.h"
#include "buffet/device_registration_info.h"
#include "buffet/exported_property_set.h"

namespace buffet {

class CommandManager;

namespace dbus_utils {
class ExportedObjectManager;
}  // namespace dbus_utils

// The Manager is responsible for global state of Buffet.  It exposes
// interfaces which affect the entire device such as device registration and
// device state.
class Manager {
 public:
  typedef base::Callback<void(bool success)> OnInitFinish;

  Manager(scoped_refptr<dbus::Bus> bus,
          base::WeakPtr<dbus_utils::ExportedObjectManager> object_manager);
  ~Manager();
  void Init(const OnInitFinish& cb);

 private:
  struct Properties: public dbus_utils::ExportedPropertySet {
   public:
    dbus_utils::ExportedProperty<std::string> state_;
    explicit Properties(dbus::Bus* bus)
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

  scoped_refptr<dbus::Bus> bus_;
  dbus::ExportedObject* exported_object_;  // weak; owned by the Bus object.
  base::WeakPtr<dbus_utils::ExportedObjectManager> object_manager_;
  scoped_ptr<Properties> properties_;

  std::shared_ptr<CommandManager> command_manager_;
  DeviceRegistrationInfo device_info_;

  DISALLOW_COPY_AND_ASSIGN(Manager);
};

}  // namespace buffet

#endif  // BUFFET_MANAGER_H_
