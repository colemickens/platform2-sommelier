// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/device_manager.h"

#include <string>

#include <base/message_loop/message_loop.h>
#include <chromeos/http/http_transport.h>

#include "libweave/src/base_api_handler.h"
#include "libweave/src/buffet_config.h"
#include "libweave/src/commands/command_manager.h"
#include "libweave/src/device_registration_info.h"
#include "libweave/src/privet/privet_manager.h"
#include "libweave/src/states/state_change_queue.h"
#include "libweave/src/states/state_manager.h"

namespace weave {

namespace {

// Max of 100 state update events should be enough in the queue.
const size_t kMaxStateChangeQueueSize = 100;
// The number of seconds each HTTP request will be allowed before timing out.
const int kRequestTimeoutSeconds = 30;

}  // namespace

DeviceManager::DeviceManager() {}

DeviceManager::~DeviceManager() {}

void DeviceManager::Start(const Options& options,
                          Network* network,
                          Mdns* mdns,
                          HttpServer* http_server) {
  command_manager_ = std::make_shared<CommandManager>();
  command_manager_->Startup(options.definitions_path,
                            options.test_definitions_path);
  state_change_queue_.reset(new StateChangeQueue(kMaxStateChangeQueueSize));
  state_manager_ = std::make_shared<StateManager>(state_change_queue_.get());
  state_manager_->Startup();

  auto transport = chromeos::http::Transport::CreateDefault();
  transport->SetDefaultTimeout(
      base::TimeDelta::FromSeconds(kRequestTimeoutSeconds));

  std::unique_ptr<BuffetConfig> config{new BuffetConfig{options.state_path}};
  config->Load(options.config_path);

  // TODO(avakulenko): Figure out security implications of storing
  // device info state data unencrypted.
  device_info_.reset(new DeviceRegistrationInfo(
      command_manager_, state_manager_, std::move(config), transport,
      base::MessageLoop::current()->task_runner(), options.xmpp_enabled,
      network));
  base_api_handler_.reset(
      new BaseApiHandler{device_info_.get(), state_manager_, command_manager_});

  device_info_->Start();

  if (!options.disable_privet) {
    StartPrivet(options, network, mdns, http_server);
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
                                Network* network,
                                Mdns* mdns,
                                HttpServer* http_server) {
  privet_.reset(new privet::Manager{});
  privet_->Start(options, network, mdns, http_server, device_info_.get(),
                 command_manager_.get(), state_manager_.get());

  privet_->AddOnWifiSetupChangedCallback(
      base::Bind(&DeviceManager::OnWiFiBootstrapStateChanged,
                 weak_ptr_factory_.GetWeakPtr()));
}

void DeviceManager::OnWiFiBootstrapStateChanged(
    weave::privet::WifiBootstrapManager::State state) {
  const std::string& ssid = privet_->GetCurrentlyConnectedSsid();
  if (ssid != device_info_->GetConfig().last_configured_ssid()) {
    weave::BuffetConfig::Transaction change{device_info_->GetMutableConfig()};
    change.set_last_configured_ssid(ssid);
  }
}

std::unique_ptr<Device> Device::Create() {
  return std::unique_ptr<Device>{new DeviceManager};
}

}  // namespace weave
