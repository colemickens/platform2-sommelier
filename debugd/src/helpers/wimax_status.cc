// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include <memory>
#include <string>
#include <utility>

#include <base/json/json_writer.h>
#include <base/memory/ptr_util.h>
#include <base/values.h>
#include <chromeos/dbus/service_constants.h>

#include "debugd/src/helpers/system_service_proxy.h"

namespace debugd {
namespace {

std::unique_ptr<base::Value> CollectWiMaxStatus() {
  auto proxy =
      SystemServiceProxy::Create(wimax_manager::kWiMaxManagerServiceName);
  if (!proxy)
    return base::MakeUnique<base::DictionaryValue>();

  // Gets the manager properties from which we can identify the list of device
  // object paths.
  auto manager_properties = proxy->GetProperties(
      wimax_manager::kWiMaxManagerInterface,
      dbus::ObjectPath(wimax_manager::kWiMaxManagerServicePath));
  if (!manager_properties)
    return base::MakeUnique<base::DictionaryValue>();

  // Gets the device properties of all listed devices.
  auto device_paths = proxy->GetObjectPaths(*manager_properties,
                                            wimax_manager::kDevicesProperty);
  auto devices = proxy->BuildObjectPropertiesMap(
      wimax_manager::kWiMaxManagerDeviceInterface, device_paths);

  // Each device is associated with a list of network object paths. Expand the
  // network object paths into network properties.
  for (const auto& device_path : device_paths) {
    base::DictionaryValue* device_properties = nullptr;
    CHECK(devices->GetDictionary(device_path.value(), &device_properties));
    auto network_paths = proxy->GetObjectPaths(
        *device_properties, wimax_manager::kNetworksProperty);
    auto networks = proxy->BuildObjectPropertiesMap(
        wimax_manager::kWiMaxManagerNetworkInterface, network_paths);
    device_properties->SetWithoutPathExpansion(wimax_manager::kNetworksProperty,
                                               std::move(device_properties));
  }

  manager_properties->SetWithoutPathExpansion(wimax_manager::kDevicesProperty,
                                              std::move(devices));
  return std::move(manager_properties);
}

}  // namespace
}  // namespace debugd

int main() {
  auto result = debugd::CollectWiMaxStatus();
  std::string json;
  base::JSONWriter::WriteWithOptions(
      *result, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  printf("%s\n", json.c_str());
  return 0;
}
