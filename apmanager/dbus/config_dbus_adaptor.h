// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_DBUS_CONFIG_DBUS_ADAPTOR_H_
#define APMANAGER_DBUS_CONFIG_DBUS_ADAPTOR_H_

#include <string>

#include <base/macros.h>
#include <brillo/errors/error.h>
#include <dbus_bindings/org.chromium.apmanager.Config.h>

#include "apmanager/config_adaptor_interface.h"

namespace apmanager {

class Config;

class ConfigDBusAdaptor
    : public org::chromium::apmanager::ConfigAdaptor,
      public org::chromium::apmanager::ConfigInterface,
      public ConfigAdaptorInterface {
 public:
  ConfigDBusAdaptor(const scoped_refptr<dbus::Bus>& bus,
                    brillo::dbus_utils::ExportedObjectManager* object_manager,
                    Config* config,
                    int service_identifier);
  virtual ~ConfigDBusAdaptor();

  // Implementation of org::chromium::apmanager::ConfigAdaptor.
  bool ValidateSsid(brillo::ErrorPtr* error,
                    const std::string& value) override;
  bool ValidateSecurityMode(brillo::ErrorPtr* error,
                            const std::string& value) override;
  bool ValidatePassphrase(brillo::ErrorPtr* error,
                          const std::string& value) override;
  bool ValidateHwMode(brillo::ErrorPtr* error,
                      const std::string& value) override;
  bool ValidateOperationMode(brillo::ErrorPtr* error,
                             const std::string& value) override;
  bool ValidateChannel(brillo::ErrorPtr* error,
                       const uint16_t& value) override;

  // Implementation of ConfigAdaptorInterface.
  RPCObjectIdentifier GetRpcObjectIdentifier() override;
  void SetSsid(const std::string& ssid) override;
  std::string GetSsid() override;
  void SetInterfaceName(const std::string& interface_name) override;
  std::string GetInterfaceName() override;
  void SetSecurityMode(const std::string& security_mode) override;
  std::string GetSecurityMode() override;
  void SetPassphrase(const std::string& passphrase) override;
  std::string GetPassphrase() override;
  void SetHwMode(const std::string& hw_mode) override;
  std::string GetHwMode() override;
  void SetOperationMode(const std::string& op_mode) override;
  std::string GetOperationMode() override;
  void SetChannel(uint16_t channel) override;
  uint16_t GetChannel() override;
  void SetHiddenNetwork(bool hidden) override;
  bool GetHiddenNetwork() override;
  void SetBridgeInterface(const std::string& interface_name) override;
  std::string GetBridgeInterface() override;
  void SetServerAddressIndex(uint16_t) override;
  uint16_t GetServerAddressIndex() override;
  void SetFullDeviceControl(bool full_control) override;
  bool GetFullDeviceControl() override;

 private:
  dbus::ObjectPath dbus_path_;
  brillo::dbus_utils::DBusObject dbus_object_;
  Config* config_;

  DISALLOW_COPY_AND_ASSIGN(ConfigDBusAdaptor);
};

}  // namespace apmanager

#endif  // APMANAGER_DBUS_CONFIG_DBUS_ADAPTOR_H_
