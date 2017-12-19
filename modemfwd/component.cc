// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modemfwd/component.h"

#include <memory>
#include <string>

#include <base/files/file_path.h>
#include <base/logging.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/message.h>
#include <dbus/object_path.h>
#include <dbus/object_proxy.h>

namespace {

constexpr char kComponentName[] = "cros-cellular";

}  // namespace

namespace modemfwd {

// static
std::unique_ptr<Component> Component::Load(scoped_refptr<dbus::Bus> bus) {
  dbus::ObjectProxy* proxy = bus->GetObjectProxy(
      chromeos::kComponentUpdaterServiceName,
      dbus::ObjectPath(chromeos::kComponentUpdaterServicePath));
  if (!proxy)
    return nullptr;

  dbus::MethodCall method_call(
      chromeos::kComponentUpdaterServiceInterface,
      chromeos::kComponentUpdaterServiceLoadComponentMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(kComponentName);
  std::unique_ptr<dbus::Response> resp = proxy->CallMethodAndBlock(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
  if (!resp) {
    LOG(ERROR) << "Failed to load component";
    return nullptr;
  }

  dbus::MessageReader reader(resp.get());
  std::string loaded_path;
  if (!reader.PopString(&loaded_path)) {
    LOG(ERROR) << "Got malformed response trying to load component";
    return nullptr;
  }

  return std::unique_ptr<Component>(
      new Component(bus, proxy, base::FilePath(loaded_path)));
}

Component::Component(scoped_refptr<dbus::Bus> bus,
                     dbus::ObjectProxy* proxy,
                     const base::FilePath& base_component_path)
    : bus_(bus), proxy_(proxy), base_component_path_(base_component_path) {}

Component::~Component() {
  Unload();
}

base::FilePath Component::GetPath() const {
  return base_component_path_;
}

void Component::Unload() {
  dbus::MethodCall method_call(
      chromeos::kComponentUpdaterServiceInterface,
      chromeos::kComponentUpdaterServiceUnloadComponentMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(kComponentName);
  std::unique_ptr<dbus::Response> resp = proxy_->CallMethodAndBlock(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
  if (!resp)
    LOG(ERROR) << "Failed to unload component";
}

}  // namespace modemfwd
