// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_SRC_DEVICE_MANAGER_H_
#define LIBWEAVE_SRC_DEVICE_MANAGER_H_

#include <base/memory/weak_ptr.h>
#include <weave/device.h>

namespace weave {

class BaseApiHandler;
class Config;
class CommandManager;
class DeviceRegistrationInfo;
class StateChangeQueue;
class StateManager;

namespace privet {
class Manager;
}  // namespace privet

class DeviceManager final : public Device {
 public:
  DeviceManager();
  ~DeviceManager() override;

  void Start(const Options& options,
             ConfigStore* config_store,
             TaskRunner* task_runner,
             HttpClient* http_client,
             Network* network,
             Mdns* mdns,
             HttpServer* http_server) override;

  Commands* GetCommands() override;
  State* GetState() override;
  Cloud* GetCloud() override;
  Privet* GetPrivet() override;

  Config* GetConfig();

 private:
  void StartPrivet(const Options& options,
                   TaskRunner* task_runner,
                   Network* network,
                   Mdns* mdns,
                   HttpServer* http_server);

  void OnWiFiBootstrapStateChanged(weave::WifiSetupState state);

  std::shared_ptr<CommandManager> command_manager_;
  std::unique_ptr<StateChangeQueue> state_change_queue_;
  std::shared_ptr<StateManager> state_manager_;
  std::unique_ptr<DeviceRegistrationInfo> device_info_;
  std::unique_ptr<BaseApiHandler> base_api_handler_;
  std::unique_ptr<privet::Manager> privet_;

  base::WeakPtrFactory<DeviceManager> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(DeviceManager);
};

}  // namespace weave

#endif  // LIBWEAVE_SRC_DEVICE_MANAGER_H_
