// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_CONFIG_ADAPTOR_INTERFACE_H_
#define APMANAGER_CONFIG_ADAPTOR_INTERFACE_H_

#include <string>

#include "apmanager/rpc_interface.h"

namespace apmanager {

class ConfigAdaptorInterface {
 public:
  virtual ~ConfigAdaptorInterface() {}

  // Returns an identifier/handle that represents this object over
  // the IPC interface (e.g. dbus::ObjectPath for D-Bus, IBinder
  // for Binder).
  virtual RPCObjectIdentifier GetRpcObjectIdentifier() = 0;

  // Getter/setter for configuration properties.
  virtual void SetSsid(const std::string& ssid) = 0;
  virtual std::string GetSsid() = 0;
  virtual void SetInterfaceName(const std::string& interface_name) = 0;
  virtual std::string GetInterfaceName() = 0;
  virtual void SetSecurityMode(const std::string& security_mode) = 0;
  virtual std::string GetSecurityMode() = 0;
  virtual void SetPassphrase(const std::string& passphrase) = 0;
  virtual std::string GetPassphrase() = 0;
  virtual void SetHwMode(const std::string& hw_mode) = 0;
  virtual std::string GetHwMode() = 0;
  virtual void SetOperationMode(const std::string& op_mode) = 0;
  virtual std::string GetOperationMode() = 0;
  virtual void SetChannel(uint16_t channel) = 0;
  virtual uint16_t GetChannel() = 0;
  virtual void SetHiddenNetwork(bool hidden) = 0;
  virtual bool GetHiddenNetwork() = 0;
  virtual void SetBridgeInterface(const std::string& interface_name) = 0;
  virtual std::string GetBridgeInterface() = 0;
  virtual void SetServerAddressIndex(uint16_t) = 0;
  virtual uint16_t GetServerAddressIndex() = 0;
  virtual void SetFullDeviceControl(bool full_control) = 0;
  virtual bool GetFullDeviceControl() = 0;
};

}  // namespace apmanager

#endif  // APMANAGER_CONFIG_ADAPTOR_INTERFACE_H_
