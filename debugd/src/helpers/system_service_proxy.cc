// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/helpers/system_service_proxy.h"

#include <base/memory/ptr_util.h>
#include <dbus/values_util.h>

namespace debugd {

namespace {

const char kDBusPropertiesInterface[] = "org.freedesktop.DBus.Properties";
const char kDBusPropertiesGetAllMethod[] = "GetAll";

}  // namespace

// static
std::unique_ptr<SystemServiceProxy> SystemServiceProxy::Create(
    const std::string& service_name) {
  scoped_refptr<dbus::Bus> bus = ConnectToSystemBus();
  if (!bus)
    return nullptr;

  return std::unique_ptr<SystemServiceProxy>(
      new SystemServiceProxy(bus, service_name));
}

SystemServiceProxy::SystemServiceProxy(scoped_refptr<dbus::Bus> bus,
                                       const std::string& service_name)
    : bus_(bus), service_name_(service_name) {}

// static
scoped_refptr<dbus::Bus> SystemServiceProxy::ConnectToSystemBus() {
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus(new dbus::Bus(options));
  if (!bus->Connect())
    return nullptr;

  return bus;
}

std::unique_ptr<base::Value> SystemServiceProxy::CallMethodAndGetResponse(
    const dbus::ObjectPath& object_path, dbus::MethodCall* method_call) {
  dbus::ObjectProxy* object_proxy =
      bus_->GetObjectProxy(service_name_, object_path);
  std::unique_ptr<dbus::Response> response = object_proxy->CallMethodAndBlock(
      method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
  if (!response)
    return nullptr;

  dbus::MessageReader reader(response.get());
  return dbus::PopDataAsValue(&reader);
}

std::unique_ptr<base::DictionaryValue> SystemServiceProxy::GetProperties(
    const std::string& interface_name, const dbus::ObjectPath& object_path) {
  dbus::MethodCall method_call(kDBusPropertiesInterface,
                               kDBusPropertiesGetAllMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(interface_name);
  return base::DictionaryValue::From(
      CallMethodAndGetResponse(object_path, &method_call));
}

std::unique_ptr<base::DictionaryValue>
SystemServiceProxy::BuildObjectPropertiesMap(
    const std::string& interface_name,
    const std::vector<dbus::ObjectPath>& object_paths) {
  auto result = std::make_unique<base::DictionaryValue>();
  for (const auto& object_path : object_paths) {
    result->SetWithoutPathExpansion(object_path.value(),
                                    GetProperties(interface_name, object_path));
  }
  return result;
}

// static
std::vector<dbus::ObjectPath> SystemServiceProxy::GetObjectPaths(
    const base::DictionaryValue& properties, const std::string& property_name) {
  std::vector<dbus::ObjectPath> object_paths;
  const base::ListValue* paths = nullptr;
  if (properties.GetList(property_name, &paths)) {
    for (size_t i = 0; i < paths->GetSize(); ++i) {
      std::string path;
      if (paths->GetString(i, &path)) {
        object_paths.emplace_back(path);
      }
    }
  }
  return object_paths;
}

}  // namespace debugd
