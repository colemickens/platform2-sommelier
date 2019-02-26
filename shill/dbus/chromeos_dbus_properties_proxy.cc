// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus/chromeos_dbus_properties_proxy.h"

#include "shill/logging.h"

namespace shill {

using std::string;
using std::vector;

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kDBus;
static string ObjectID(const dbus::ObjectPath* p) { return p->value(); }
}

ChromeosDBusPropertiesProxy::ChromeosDBusPropertiesProxy(
    const scoped_refptr<dbus::Bus>& bus,
    const string& path,
    const string& service)
    : proxy_(
        new org::freedesktop::DBus::PropertiesProxy(
            bus, service, dbus::ObjectPath(path))) {
  // Register signal handlers.
  proxy_->RegisterPropertiesChangedSignalHandler(
      base::Bind(&ChromeosDBusPropertiesProxy::PropertiesChanged,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&ChromeosDBusPropertiesProxy::OnSignalConnected,
                 weak_factory_.GetWeakPtr()));
  proxy_->RegisterMmPropertiesChangedSignalHandler(
      base::Bind(&ChromeosDBusPropertiesProxy::MmPropertiesChanged,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&ChromeosDBusPropertiesProxy::OnSignalConnected,
                 weak_factory_.GetWeakPtr()));
}

ChromeosDBusPropertiesProxy::~ChromeosDBusPropertiesProxy() {}

KeyValueStore ChromeosDBusPropertiesProxy::GetAll(
    const string& interface_name) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__ << "(" << interface_name << ")";
  brillo::VariantDictionary properties_dict;
  brillo::ErrorPtr error;
  if (!proxy_->GetAll(interface_name, &properties_dict, &error)) {
    LOG(ERROR) << __func__ << " failed on " << interface_name
               << ": " << error->GetCode() << " " << error->GetMessage();
    return KeyValueStore();
  }
  KeyValueStore properties_store =
      KeyValueStore::ConvertFromVariantDictionary(properties_dict);
  return properties_store;
}

brillo::Any ChromeosDBusPropertiesProxy::Get(const string& interface_name,
                                             const string& property) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__ << "(" << interface_name
      << ", " << property << ")";
  brillo::Any value;
  brillo::ErrorPtr error;
  if (!proxy_->Get(interface_name, property, &value, &error)) {
    LOG(ERROR) << __func__ << " failed for " << interface_name
               << " " << property << ": "
               << error->GetCode() << " " << error->GetMessage();
  }
  return value;
}

void ChromeosDBusPropertiesProxy::MmPropertiesChanged(
    const string& interface,
    const brillo::VariantDictionary& properties) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__ << "(" << interface << ")";
  KeyValueStore properties_store =
      KeyValueStore::ConvertFromVariantDictionary(properties);
  mm_properties_changed_callback_.Run(interface, properties_store);
}

void ChromeosDBusPropertiesProxy::PropertiesChanged(
    const string& interface,
    const brillo::VariantDictionary& changed_properties,
    const vector<string>& invalidated_properties) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__ << "(" << interface << ")";
  KeyValueStore properties_store =
      KeyValueStore::ConvertFromVariantDictionary(changed_properties);
  properties_changed_callback_.Run(
      interface, properties_store, invalidated_properties);
}

void ChromeosDBusPropertiesProxy::OnSignalConnected(
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
