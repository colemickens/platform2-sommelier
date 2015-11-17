//
// Copyright (C) 2014 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "apmanager/dbus/config_dbus_adaptor.h"

#include <base/strings/stringprintf.h>
#include <dbus_bindings/org.chromium.apmanager.Manager.h>

#if !defined(__ANDROID__)
#include <chromeos/dbus/service_constants.h>
#else
#include <dbus/apmanager/dbus-constants.h>
#endif  // __ANDROID__

#include "apmanager/config.h"

using brillo::dbus_utils::ExportedObjectManager;
using brillo::ErrorPtr;
using org::chromium::apmanager::ConfigAdaptor;
using org::chromium::apmanager::ManagerAdaptor;
using std::string;

namespace apmanager {

ConfigDBusAdaptor::ConfigDBusAdaptor(
    const scoped_refptr<dbus::Bus>& bus,
    ExportedObjectManager* object_manager,
    Config* config,
    int service_identifier)
    : org::chromium::apmanager::ConfigAdaptor(this),
      dbus_path_(
          base::StringPrintf("%s/services/%d/config",
                             ManagerAdaptor::GetObjectPath().value().c_str(),
                             service_identifier)),
      dbus_object_(object_manager, bus, dbus_path_),
      config_(config) {
  // Register D-Bus object.
  dbus_object_.RegisterAndBlock();
}

ConfigDBusAdaptor::~ConfigDBusAdaptor() {}

bool ConfigDBusAdaptor::ValidateSsid(ErrorPtr* error, const string& value) {
  // TODO(zqiu): To be implemented.
  return true;
}

bool ConfigDBusAdaptor::ValidateSecurityMode(ErrorPtr* error,
                                             const string& value) {
  // TODO(zqiu): To be implemented.
  return true;
}

bool ConfigDBusAdaptor::ValidatePassphrase(ErrorPtr* error,
                                           const string& value) {
  // TODO(zqiu): To be implemented.
  return true;
}

bool ConfigDBusAdaptor::ValidateHwMode(ErrorPtr* error, const string& value) {
  // TODO(zqiu): To be implemented.
  return true;
}

bool ConfigDBusAdaptor::ValidateOperationMode(ErrorPtr* error,
                                              const string& value) {
  // TODO(zqiu): To be implemented.
  return true;
}

bool ConfigDBusAdaptor::ValidateChannel(ErrorPtr* error,
                                        const uint16_t& value) {
  // TODO(zqiu): To be implemented.
  return true;
}

void ConfigDBusAdaptor::SetSsid(const string& ssid) {
  ConfigAdaptor::SetSsid(ssid);
}

string ConfigDBusAdaptor::GetSsid() {
  return ConfigAdaptor::GetSsid();
}

void ConfigDBusAdaptor::SetInterfaceName(const std::string& interface_name) {
  ConfigAdaptor::SetInterfaceName(interface_name);
}

string ConfigDBusAdaptor::GetInterfaceName() {
  return ConfigAdaptor::GetInterfaceName();
}

void ConfigDBusAdaptor::SetSecurityMode(const std::string& mode) {
  ConfigAdaptor::SetSecurityMode(mode);
}

string ConfigDBusAdaptor::GetSecurityMode() {
  return ConfigAdaptor::GetSecurityMode();
}

void ConfigDBusAdaptor::SetPassphrase(const std::string& passphrase) {
  ConfigAdaptor::SetPassphrase(passphrase);
}

string ConfigDBusAdaptor::GetPassphrase() {
  return ConfigAdaptor::GetPassphrase();
}

void ConfigDBusAdaptor::SetHwMode(const std::string& hw_mode) {
  ConfigAdaptor::SetHwMode(hw_mode);
}

string ConfigDBusAdaptor::GetHwMode() {
  return ConfigAdaptor::GetHwMode();
}

void ConfigDBusAdaptor::SetOperationMode(const std::string& op_mode) {
  ConfigAdaptor::SetOperationMode(op_mode);
}

string ConfigDBusAdaptor::GetOperationMode() {
  return ConfigAdaptor::GetOperationMode();
}

void ConfigDBusAdaptor::SetChannel(uint16_t channel) {
  ConfigAdaptor::SetChannel(channel);
}

uint16_t ConfigDBusAdaptor::GetChannel() {
  return ConfigAdaptor::GetChannel();
}

void ConfigDBusAdaptor::SetHiddenNetwork(bool hidden_network) {
  ConfigAdaptor::SetHiddenNetwork(hidden_network);
}

bool ConfigDBusAdaptor::GetHiddenNetwork() {
  return ConfigAdaptor::GetHiddenNetwork();
}

void ConfigDBusAdaptor::SetBridgeInterface(const std::string& interface_name) {
  ConfigAdaptor::SetBridgeInterface(interface_name);
}

string ConfigDBusAdaptor::GetBridgeInterface() {
  return ConfigAdaptor::GetBridgeInterface();
}

void ConfigDBusAdaptor::SetServerAddressIndex(uint16_t index) {
  ConfigAdaptor::SetServerAddressIndex(index);
}

uint16_t ConfigDBusAdaptor::GetServerAddressIndex() {
  return ConfigAdaptor::GetServerAddressIndex();
}

void ConfigDBusAdaptor::SetFullDeviceControl(bool full_control) {
  ConfigAdaptor::SetFullDeviceControl(full_control);
}

bool ConfigDBusAdaptor::GetFullDeviceControl() {
  return ConfigAdaptor::GetFullDeviceControl();
}

}  // namespace apmanager
