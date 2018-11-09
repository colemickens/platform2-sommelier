// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/fake_config_adaptor.h"

using std::string;

namespace apmanager {

FakeConfigAdaptor::FakeConfigAdaptor() {}

FakeConfigAdaptor::~FakeConfigAdaptor() {}

RPCObjectIdentifier FakeConfigAdaptor::GetRpcObjectIdentifier() {
  return RPCObjectIdentifier();
}

void FakeConfigAdaptor::SetSsid(const string& ssid) {
  ssid_ = ssid;
}

string FakeConfigAdaptor::GetSsid() {
  return ssid_;
}

void FakeConfigAdaptor::SetInterfaceName(const std::string& interface_name) {
  interface_name_ = interface_name;
}

string FakeConfigAdaptor::GetInterfaceName() {
  return interface_name_;
}

void FakeConfigAdaptor::SetSecurityMode(const std::string& mode) {
  security_mode_ = mode;
}

string FakeConfigAdaptor::GetSecurityMode() {
  return security_mode_;
}

void FakeConfigAdaptor::SetPassphrase(const std::string& passphrase) {
  passphrase_ = passphrase;
}

string FakeConfigAdaptor::GetPassphrase() {
  return passphrase_;
}

void FakeConfigAdaptor::SetHwMode(const std::string& hw_mode) {
  hw_mode_ = hw_mode;
}

string FakeConfigAdaptor::GetHwMode() {
  return hw_mode_;
}

void FakeConfigAdaptor::SetOperationMode(const std::string& op_mode) {
  op_mode_ = op_mode;
}

string FakeConfigAdaptor::GetOperationMode() {
  return op_mode_;
}

void FakeConfigAdaptor::SetChannel(uint16_t channel) {
  channel_ = channel;
}

uint16_t FakeConfigAdaptor::GetChannel() {
  return channel_;
}

void FakeConfigAdaptor::SetHiddenNetwork(bool hidden_network) {
  hidden_network_ = hidden_network;
}

bool FakeConfigAdaptor::GetHiddenNetwork() {
  return hidden_network_;
}

void FakeConfigAdaptor::SetBridgeInterface(const std::string& interface_name) {
  bridge_interface_ = interface_name;
}

string FakeConfigAdaptor::GetBridgeInterface() {
  return bridge_interface_;
}

void FakeConfigAdaptor::SetServerAddressIndex(uint16_t index) {
  server_address_index_ = index;
}

uint16_t FakeConfigAdaptor::GetServerAddressIndex() {
  return server_address_index_;
}

void FakeConfigAdaptor::SetFullDeviceControl(bool full_control) {
  full_device_control_ = full_control;
}

bool FakeConfigAdaptor::GetFullDeviceControl() {
  return full_device_control_;
}

}  // namespace apmanager
