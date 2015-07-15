// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_SRC_DEVICE_MANAGER_H_
#define LIBWEAVE_SRC_DEVICE_MANAGER_H_

#include <base/memory/weak_ptr.h>

#include "weave/device.h"

namespace chromeos {
namespace dbus_utils {
class DBusObject;
}
}

namespace weave {

class BaseApiHandler;
class BuffetConfig;
class CommandManager;
class DeviceRegistrationInfo;
class StateChangeQueue;
class StateManager;

namespace privet {
class ShillClient;
class Manager;
}  // namespace privet

class DeviceManager final : public Device {
 public:
  DeviceManager();
  ~DeviceManager() override;

  void Start(const Options& options,
             chromeos::dbus_utils::DBusObject* dbus_object,
             chromeos::dbus_utils::AsyncEventSequencer* sequencer) override;

  Commands* GetCommands() override;
  State* GetState() override;
  Config* GetConfig() override;
  Cloud* GetCloud() override;
  Privet* GetPrivet() override;

 private:
  void StartPrivet(const Options& options,
                   chromeos::dbus_utils::DBusObject* dbus_object,
                   chromeos::dbus_utils::AsyncEventSequencer* sequencer);

  void OnWiFiBootstrapStateChanged(weave::WifiSetupState state);

  std::shared_ptr<CommandManager> command_manager_;
  std::unique_ptr<StateChangeQueue> state_change_queue_;
  std::shared_ptr<StateManager> state_manager_;
  std::unique_ptr<DeviceRegistrationInfo> device_info_;
  std::unique_ptr<BaseApiHandler> base_api_handler_;
  std::unique_ptr<privet::ShillClient> shill_client_;
  std::unique_ptr<privet::Manager> privet_;

  base::WeakPtrFactory<DeviceManager> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(DeviceManager);
};

}  // namespace weave

#endif  // LIBWEAVE_SRC_DEVICE_MANAGER_H_
