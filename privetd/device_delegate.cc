// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privetd/device_delegate.h"

#include <set>

#include <base/files/file_util.h>
#include <base/guid.h>

#include "privetd/constants.h"
#include "privetd/daemon_state.h"

namespace privetd {

namespace {

const char kDefaultDeviceName[] = "Unnamed Device";

class DeviceDelegateImpl : public DeviceDelegate {
 public:
  DeviceDelegateImpl(uint16_t http_port,
                     uint16_t https_port,
                     DaemonState* state_store,
                     const base::Closure& on_changed)
      : http_port_(http_port),
        https_port_(https_port),
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
    return name.empty() ? kDefaultDeviceName : name;
  }
  std::string GetDescription() const override {
    std::string description;
    state_store_->GetString(state_key::kDeviceDescription, &description);
    return description;
  }
  std::string GetLocation() const override {
    std::string location;
    state_store_->GetString(state_key::kDeviceLocation, &location);
    return location;
  }
  std::string GetClass() const override { return "BB"; }
  std::string GetModelId() const override {
    return "///";  // No model id.
  }
  std::vector<std::string> GetServices() const override {
    return std::vector<std::string>(types_.begin(), types_.end());
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
  void AddType(const std::string& type) override {
    types_.insert(type);
    on_changed_.Run();
  }
  void RemoveType(const std::string& type) override {
    types_.erase(type);
    on_changed_.Run();
  }

 private:
  const uint16_t http_port_;
  const uint16_t https_port_;
  DaemonState* state_store_;
  base::Closure on_changed_;
  base::Time start_time_ = base::Time::Now();
  std::set<std::string> types_;
};

}  // namespace

DeviceDelegate::DeviceDelegate() {
}

DeviceDelegate::~DeviceDelegate() {
}

// static
std::unique_ptr<DeviceDelegate> DeviceDelegate::CreateDefault(
    uint16_t http_port,
    uint16_t https_port,
    DaemonState* state_store,
    const base::Closure& on_changed) {
  return std::unique_ptr<DeviceDelegate>(
      new DeviceDelegateImpl(http_port, https_port, state_store, on_changed));
}

}  // namespace privetd
