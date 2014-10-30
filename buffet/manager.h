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
class StateChangeQueue;
class StateManager;

// The Manager is responsible for global state of Buffet.  It exposes
// interfaces which affect the entire device such as device registration and
// device state.
class Manager final {
 public:
  explicit Manager(
      const base::WeakPtr<chromeos::dbus_utils::ExportedObjectManager>&
          object_manager);
  ~Manager();

  void RegisterAsync(
      const chromeos::dbus_utils::AsyncEventSequencer::CompletionAction& cb);

 private:
  // DBus methods:
  // Handles calls to org.chromium.Buffet.Manager.StartDevice().
  void HandleStartDevice(
      scoped_ptr<chromeos::dbus_utils::DBusMethodResponse> response);
  // Handles calls to org.chromium.Buffet.Manager.CheckDeviceRegistered().
  void HandleCheckDeviceRegistered(
      scoped_ptr<chromeos::dbus_utils::DBusMethodResponse> response);
  // Handles calls to org.chromium.Buffet.Manager.GetDeviceInfo().
  void HandleGetDeviceInfo(
      scoped_ptr<chromeos::dbus_utils::DBusMethodResponse> response);
  // Handles calls to org.chromium.Buffet.Manager.RegisterDevice().
  void HandleRegisterDevice(
      scoped_ptr<chromeos::dbus_utils::DBusMethodResponse> response,
      const std::map<std::string, std::string>& params);
  // Handles calls to org.chromium.Buffet.Manager.UpdateState().
  void HandleUpdateState(
      scoped_ptr<chromeos::dbus_utils::DBusMethodResponse> response,
      const chromeos::VariantDictionary& property_set);
  // Handles calls to org.chromium.Buffet.Manager.AddCommand().
  void HandleAddCommand(
      scoped_ptr<chromeos::dbus_utils::DBusMethodResponse> response,
      const std::string& json_command);
  // Handles calls to org.chromium.Buffet.Manager.Test()
  std::string HandleTestMethod(const std::string& message);

  chromeos::dbus_utils::DBusObject dbus_object_;

  std::shared_ptr<CommandManager> command_manager_;
  std::unique_ptr<StateChangeQueue> state_change_queue_;
  std::shared_ptr<StateManager> state_manager_;
  std::unique_ptr<DeviceRegistrationInfo> device_info_;

  DISALLOW_COPY_AND_ASSIGN(Manager);
};

}  // namespace buffet

#endif  // BUFFET_MANAGER_H_
