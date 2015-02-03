// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privetd/device_delegate.h"

#include <base/files/file_util.h>
#include <base/guid.h>

#include "privetd/constants.h"
#include "privetd/daemon_state.h"
#include "privetd/privetd_conf_parser.h"

namespace privetd {

namespace {

class DeviceDelegateImpl : public DeviceDelegate {
 public:
  DeviceDelegateImpl(const PrivetdConfigParser* config,
                     DaemonState* state_store,
                     const base::Closure& on_changed)
      : config_(config),
        state_store_(state_store),
        on_changed_(on_changed) {
    if (GetId().empty()) {
      // TODO(wiley) This should probably be consistent with the peerd UUID.
      state_store_->SetString(state_key::kDeviceId, base::GenerateGUID());
      state_store_->Save();
    }
  }
  ~DeviceDelegateImpl() override {}

  // DeviceDelegate methods.
  std::string GetId() const override {
    std::string id;
    state_store_->GetString(state_key::kDeviceId, &id);
    return id;
  }
  std::string GetName() const override {
    std::string name;
    state_store_->GetString(state_key::kDeviceName, &name);
    return name.empty() ? config_->device_name() : name;
  }
  std::string GetDescription() const override {
    std::string description;
    if (!state_store_->GetString(state_key::kDeviceDescription, &description))
      return config_->device_description();
    return description;
  }
  std::string GetLocation() const override {
    std::string location;
    state_store_->GetString(state_key::kDeviceLocation, &location);
    return location;
  }
  std::string GetClass() const override { return config_->device_class(); }
  std::string GetModelId() const override { return config_->device_model_id(); }
  std::vector<std::string> GetServices() const override {
    return config_->device_services();
  }
  std::pair<uint16_t, uint16_t> GetHttpEnpoint() const override {
    return std::make_pair(http_port_, http_port_);
  }
  std::pair<uint16_t, uint16_t> GetHttpsEnpoint() const override {
    return std::make_pair(https_port_, https_port_);
  }
  base::TimeDelta GetUptime() const override {
    return base::Time::Now() - start_time_;
  }
  void SetName(const std::string& name) override {
    state_store_->SetString(state_key::kDeviceName, name);
    state_store_->Save();
    on_changed_.Run();
  }
  void SetDescription(const std::string& description) override {
    state_store_->SetString(state_key::kDeviceDescription, description);
    state_store_->Save();
    on_changed_.Run();
  }
  void SetLocation(const std::string& location) override {
    state_store_->SetString(state_key::kDeviceLocation, location);
    state_store_->Save();
    on_changed_.Run();
  }

  void SetHttpPort(uint16_t port) override {
    http_port_ = port;
  }

  void SetHttpsPort(uint16_t port) override {
    https_port_ = port;
  }

 private:
  uint16_t http_port_{0};
  uint16_t https_port_{0};
  const PrivetdConfigParser* config_{nullptr};
  DaemonState* state_store_{nullptr};
  base::Closure on_changed_;
  base::Time start_time_{base::Time::Now()};
};

}  // namespace

DeviceDelegate::DeviceDelegate() {
}

DeviceDelegate::~DeviceDelegate() {
}

// static
std::unique_ptr<DeviceDelegate> DeviceDelegate::CreateDefault(
    PrivetdConfigParser* config,
    DaemonState* state_store,
    const base::Closure& on_changed) {
  return std::unique_ptr<DeviceDelegate>(
      new DeviceDelegateImpl(config, state_store, on_changed));
}

}  // namespace privetd
