// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privetd/device_delegate.h"

#include <set>

#include <base/files/file_util.h>
#include <base/guid.h>
#include <chromeos/key_value_store.h>

namespace privetd {

namespace {

const char kConfigPath[] = "/var/lib/privetd/privetd.conf";

const char kIdKey[] = "id";
const char kNameKey[] = "name";
const char kDescriptionKey[] = "description";
const char kLocationKey[] = "description";

const char kDefaultDeviceName[] = "Unnamed Device";

class DeviceDelegateImpl : public DeviceDelegate {
 public:
  DeviceDelegateImpl(uint16_t http_port, uint16_t https_port)
      : http_port_(http_port), https_port_(https_port) {
    config_store_.Load(base::FilePath(kConfigPath));
    if (GetId().empty()) {
      config_store_.SetString(kIdKey, base::GenerateGUID());
      SaveConfig();
    }
  }
  ~DeviceDelegateImpl() override {}

  // DeviceDelegate methods.
  std::string GetId() const override {
    std::string id;
    config_store_.GetString(kIdKey, &id);
    return id;
  }
  std::string GetName() const override {
    std::string name;
    config_store_.GetString(kNameKey, &name);
    return name.empty() ? kDefaultDeviceName : name;
  }
  std::string GetDescription() const override {
    std::string description;
    config_store_.GetString(kDescriptionKey, &description);
    return description;
  }
  std::string GetLocation() const override {
    std::string location;
    config_store_.GetString(kLocationKey, &location);
    return location;
  }
  std::vector<std::string> GetTypes() const override {
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
    config_store_.SetString(kNameKey, name);
    SaveConfig();
  }
  void SetDescription(const std::string& description) override {
    config_store_.SetString(kDescriptionKey, description);
    SaveConfig();
  }
  void SetLocation(const std::string& location) override {
    config_store_.SetString(kLocationKey, location);
    SaveConfig();
  }
  void AddType(const std::string& type) override { types_.insert(type); }
  void RemoveType(const std::string& type) override { types_.erase(type); }

 private:
  void SaveConfig() {
    base::FilePath path(kConfigPath);
    CHECK(base::CreateDirectory(path.DirName()));
    CHECK(config_store_.Save(path));
  }

  chromeos::KeyValueStore config_store_;
  uint16_t http_port_;
  uint16_t https_port_;
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
    uint16_t https_port) {
  return std::unique_ptr<DeviceDelegate>(
      new DeviceDelegateImpl(http_port, https_port));
}

}  // namespace privetd
