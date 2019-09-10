// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/newblued/newblue_debug_manager.h"

#include <utility>

#include <base/bind.h>
#include <base/files/file_util.h>
#include <chromeos/dbus/service_constants.h>

namespace bluetooth {

namespace {

constexpr uint8_t kNewblueMinimumVerbosityLevel = 0;

}  // namespace

NewblueDebugManager::NewblueDebugManager(scoped_refptr<dbus::Bus> bus)
    : bus_(bus), weak_ptr_factory_(this) {}

void NewblueDebugManager::Init() {
  bus_->GetObjectManager(
          bluetooth_object_manager::kBluetoothObjectManagerServiceName,
          dbus::ObjectPath(
              bluetooth_object_manager::kBluetoothObjectManagerServicePath))
      ->RegisterInterface(bluetooth_debug::kBluetoothDebugInterface, this);
}

dbus::PropertySet* NewblueDebugManager::CreateProperties(
    dbus::ObjectProxy* object_proxy,
    const dbus::ObjectPath& object_path,
    const std::string& interface) {
  dbus::PropertySet* properties =
      new dbus::PropertySet(object_proxy, interface,
                            base::Bind(&NewblueDebugManager::OnPropertyChanged,
                                       weak_ptr_factory_.GetWeakPtr()));
  properties->RegisterProperty(bluetooth_debug::kNewblueLevelProperty,
                               &newblue_level_);
  return properties;
}

void NewblueDebugManager::OnPropertyChanged(const std::string& prop_name) {
  if (prop_name != bluetooth_debug::kNewblueLevelProperty)
    return;

  if (newblue_level_.is_valid())
    SetNewblueLogLevel(newblue_level_.value());
}

void NewblueDebugManager::SetNewblueLogLevel(int verbosity) {
  if (verbosity < kNewblueMinimumVerbosityLevel) {
    LOG(WARNING) << "Invalid verbosity level for newblue";
    return;
  }

  if (current_verbosity_ == verbosity)
    return;

  current_verbosity_ = verbosity;
  LOG(INFO) << "Log level is set to " << verbosity;
  logging::SetMinLogLevel(-verbosity);
}

}  // namespace bluetooth
