// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_MANAGER_H_
#define BUFFET_MANAGER_H_

#include <map>
#include <memory>
#include <string>

#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <base/values.h>
#include <chromeos/dbus/data_serialization.h>
#include <chromeos/dbus/dbus_object.h>
#include <chromeos/dbus/exported_property_set.h>
#include <chromeos/errors/error.h>

#include "buffet/device_registration_info.h"

namespace chromeos {
namespace dbus_utils {
class ExportedObjectManager;
}  // namespace dbus_utils
}  // namespace chromeos

namespace buffet {

class CommandManager;
class StateManager;

// The Manager is responsible for global state of Buffet.  It exposes
// interfaces which affect the entire device such as device registration and
// device state.
class Manager final {
 public:
  explicit Manager(
      const base::WeakPtr<chromeos::dbus_utils::ExportedObjectManager>&
          object_manager);
  void RegisterAsync(
      const chromeos::dbus_utils::AsyncEventSequencer::CompletionAction& cb);

 private:
  // DBus methods:
  // Handles calls to org.chromium.Buffet.Manager.StartDevice().
  void HandleStartDevice(chromeos::ErrorPtr* error);
  // Handles calls to org.chromium.Buffet.Manager.CheckDeviceRegistered().
  std::string HandleCheckDeviceRegistered(chromeos::ErrorPtr* error);
  // Handles calls to org.chromium.Buffet.Manager.GetDeviceInfo().
  std::string HandleGetDeviceInfo(chromeos::ErrorPtr* error);
  // Handles calls to org.chromium.Buffet.Manager.StartRegisterDevice().
  std::string HandleStartRegisterDevice(chromeos::ErrorPtr* error,
                                        const std::map<std::string,
                                                       std::string>& params);
  // Handles calls to org.chromium.Buffet.Manager.FinishRegisterDevice().
  std::string HandleFinishRegisterDevice(chromeos::ErrorPtr* error);
  // Handles calls to org.chromium.Buffet.Manager.UpdateState().
  void HandleUpdateState(chromeos::ErrorPtr* error,
                         const chromeos::VariantDictionary& property_set);
  // Handles calls to org.chromium.Buffet.Manager.AddCommand().
  void HandleAddCommand(chromeos::ErrorPtr* error,
                        const std::string& json_command);
  // Handles calls to org.chromium.Buffet.Manager.Test()
  std::string HandleTestMethod(chromeos::ErrorPtr* error,
                               const std::string& message);

  chromeos::dbus_utils::DBusObject dbus_object_;

  std::shared_ptr<CommandManager> command_manager_;
  std::shared_ptr<StateManager> state_manager_;
  std::unique_ptr<DeviceRegistrationInfo> device_info_;

  DISALLOW_COPY_AND_ASSIGN(Manager);
};

}  // namespace buffet

#endif  // BUFFET_MANAGER_H_
