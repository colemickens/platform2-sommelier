// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/device_manager.h"

#include <string>

#include "libweave/src/base_api_handler.h"
#include "libweave/src/commands/command_manager.h"
#include "libweave/src/config.h"
#include "libweave/src/device_registration_info.h"
#include "libweave/src/privet/privet_manager.h"
#include "libweave/src/states/state_change_queue.h"
#include "libweave/src/states/state_manager.h"

namespace weave {

namespace {

// Max of 100 state update events should be enough in the queue.
const size_t kMaxStateChangeQueueSize = 100;

}  // namespace

DeviceManager::DeviceManager() {}

DeviceManager::~DeviceManager() {}

void DeviceManager::Start(const Options& options,
                          ConfigStore* config_store,
                          TaskRunner* task_runner,
                          HttpClient* http_client,
                          Network* network,
                          Mdns* mdns,
                          HttpServer* http_server) {
  command_manager_ = std::make_shared<CommandManager>();
  command_manager_->Startup(config_store);
  state_change_queue_.reset(new StateChangeQueue(kMaxStateChangeQueueSize));
  state_manager_ = std::make_shared<StateManager>(state_change_queue_.get());
  state_manager_->Startup(config_store);

  std::unique_ptr<Config> config{new Config{config_store}};
  config->Load();

  // TODO(avakulenko): Figure out security implications of storing
  // device info state data unencrypted.
  device_info_.reset(new DeviceRegistrationInfo(
      command_manager_, state_manager_, std::move(config), task_runner,
      http_client, options.xmpp_enabled, network));
  base_api_handler_.reset(
      new BaseApiHandler{device_info_.get(), state_manager_, command_manager_});

  device_info_->Start();

  if (!options.disable_privet) {
    StartPrivet(options, task_runner, network, mdns, http_server);
  } else {
    CHECK(!http_server);
    CHECK(!mdns);
  }
}

Commands* DeviceManager::GetCommands() {
  return command_manager_.get();
}

State* DeviceManager::GetState() {
  return state_manager_.get();
}

Config* DeviceManager::GetConfig() {
  return device_info_->GetMutableConfig();
}

Cloud* DeviceManager::GetCloud() {
  return device_info_.get();
}

Privet* DeviceManager::GetPrivet() {
  return privet_.get();
}

void DeviceManager::StartPrivet(const Options& options,
                                TaskRunner* task_runner,
                                Network* network,
                                Mdns* mdns,
                                HttpServer* http_server) {
  privet_.reset(new privet::Manager{});
  privet_->Start(options, task_runner, network, mdns, http_server,
                 device_info_.get(), command_manager_.get(),
                 state_manager_.get());

  privet_->AddOnWifiSetupChangedCallback(
      base::Bind(&DeviceManager::OnWiFiBootstrapStateChanged,
                 weak_ptr_factory_.GetWeakPtr()));
}

void DeviceManager::OnWiFiBootstrapStateChanged(
    weave::privet::WifiBootstrapManager::State state) {
  const std::string& ssid = privet_->GetCurrentlyConnectedSsid();
  if (ssid != device_info_->GetConfig().last_configured_ssid()) {
    weave::Config::Transaction change{device_info_->GetMutableConfig()};
    change.set_last_configured_ssid(ssid);
  }
}

std::unique_ptr<Device> Device::Create() {
  return std::unique_ptr<Device>{new DeviceManager};
}

}  // namespace weave
