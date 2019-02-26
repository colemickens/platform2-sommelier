// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus/chromeos_modem_manager_proxy.h"

#include "shill/cellular/modem_manager.h"
#include "shill/event_dispatcher.h"
#include "shill/logging.h"

using std::string;
using std::vector;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kDBus;
static string ObjectID(const dbus::ObjectPath* p) { return p->value(); }
}  // namespace Logging

ChromeosModemManagerProxy::ChromeosModemManagerProxy(
    EventDispatcher* dispatcher,
    const scoped_refptr<dbus::Bus>& bus,
    ModemManagerClassic* manager,
    const std::string& path,
    const std::string& service,
    const base::Closure& service_appeared_callback,
    const base::Closure& service_vanished_callback)
    : proxy_(
        new org::freedesktop::ModemManagerProxy(
            bus, service, dbus::ObjectPath(path))),
      dispatcher_(dispatcher),
      manager_(manager),
      service_appeared_callback_(service_appeared_callback),
      service_vanished_callback_(service_vanished_callback),
      service_available_(false) {
  // Register signal handlers.
  proxy_->RegisterDeviceAddedSignalHandler(
      base::Bind(&ChromeosModemManagerProxy::DeviceAdded,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&ChromeosModemManagerProxy::OnSignalConnected,
                 weak_factory_.GetWeakPtr()));
  proxy_->RegisterDeviceRemovedSignalHandler(
      base::Bind(&ChromeosModemManagerProxy::DeviceRemoved,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&ChromeosModemManagerProxy::OnSignalConnected,
                 weak_factory_.GetWeakPtr()));

  // Monitor service owner changes. This callback lives for the lifetime of
  // the ObjectProxy.
  proxy_->GetObjectProxy()->SetNameOwnerChangedCallback(
      base::Bind(&ChromeosModemManagerProxy::OnServiceOwnerChanged,
                 weak_factory_.GetWeakPtr()));

  // One time callback when service becomes available.
  proxy_->GetObjectProxy()->WaitForServiceToBeAvailable(
      base::Bind(&ChromeosModemManagerProxy::OnServiceAvailable,
                 weak_factory_.GetWeakPtr()));
}

ChromeosModemManagerProxy::~ChromeosModemManagerProxy() {}

vector<string> ChromeosModemManagerProxy::EnumerateDevices() {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  if (!service_available_) {
    LOG(ERROR) << "Service not available";
    return vector<string>();
  }

  vector<dbus::ObjectPath> device_paths;
  brillo::ErrorPtr error;
  if (!proxy_->EnumerateDevices(&device_paths, &error)) {
    LOG(ERROR) << "Failed to enumerate devices: " << error->GetCode()
               << " " << error->GetMessage();
    return vector<string>();
  }
  vector<string> device_rpcids =
      KeyValueStore::ConvertPathsToRpcIdentifiers(device_paths);
  return device_rpcids;
}

void ChromeosModemManagerProxy::DeviceAdded(const dbus::ObjectPath& device) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  manager_->OnDeviceAdded(device.value());
}

void ChromeosModemManagerProxy::DeviceRemoved(const dbus::ObjectPath& device) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  manager_->OnDeviceRemoved(device.value());
}

void ChromeosModemManagerProxy::OnServiceAppeared() {
  if (!service_appeared_callback_.is_null()) {
    service_appeared_callback_.Run();
  }
}

void ChromeosModemManagerProxy::OnServiceVanished() {
  if (!service_vanished_callback_.is_null()) {
    service_vanished_callback_.Run();
  }
}

void ChromeosModemManagerProxy::OnServiceAvailable(bool available) {
  LOG(INFO) << __func__ << ": " << available;

  // The callback might invoke calls to the ObjectProxy, so defer the callback
  // to event loop.
  if (available) {
    dispatcher_->PostTask(
        FROM_HERE,
        base::Bind(&ChromeosModemManagerProxy::OnServiceAppeared,
                   weak_factory_.GetWeakPtr()));
  } else if (!available) {
    dispatcher_->PostTask(
        FROM_HERE,
        base::Bind(&ChromeosModemManagerProxy::OnServiceVanished,
                   weak_factory_.GetWeakPtr()));
  }
  service_available_ = available;
}

void ChromeosModemManagerProxy::OnServiceOwnerChanged(
    const string& old_owner, const string& new_owner) {
  LOG(INFO) << __func__ << " old: " << old_owner << " new: " << new_owner;
  if (new_owner.empty()) {
    OnServiceAvailable(false);
  } else {
    OnServiceAvailable(true);
  }
}

void ChromeosModemManagerProxy::OnSignalConnected(
    const string& interface_name, const string& signal_name, bool success) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__
      << "interface: " << interface_name
             << " signal: " << signal_name << "success: " << success;
  if (!success) {
    LOG(ERROR) << "Failed to connect signal " << signal_name
        << " to interface " << interface_name;
  }
}

}  // namespace shill
