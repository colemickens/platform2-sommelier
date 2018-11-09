// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_FAKE_CONFIG_ADAPTOR_H_
#define APMANAGER_FAKE_CONFIG_ADAPTOR_H_

#include <string>

#include <base/macros.h>

#include "apmanager/config_adaptor_interface.h"

namespace apmanager {

class FakeConfigAdaptor : public ConfigAdaptorInterface {
 public:
  FakeConfigAdaptor();
  ~FakeConfigAdaptor() override;

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
  std::string ssid_;
  std::string interface_name_;
  std::string security_mode_;
  std::string passphrase_;
  std::string hw_mode_;
  std::string op_mode_;
  std::string bridge_interface_;
  bool hidden_network_;
  bool full_device_control_;
  uint16_t channel_;
  uint16_t server_address_index_;

  DISALLOW_COPY_AND_ASSIGN(FakeConfigAdaptor);
};

}  // namespace apmanager

#endif  // APMANAGER_FAKE_CONFIG_ADAPTOR_H_
