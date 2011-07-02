// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/device.h"

#include <time.h>
#include <stdio.h>

#include <string>
#include <vector>

#include <base/logging.h>
#include <base/memory/ref_counted.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/control_interface.h"
#include "shill/device_dbus_adaptor.h"
#include "shill/dhcp_config.h"
#include "shill/dhcp_provider.h"
#include "shill/error.h"
#include "shill/manager.h"
#include "shill/property_accessor.h"
#include "shill/refptr_types.h"
#include "shill/rtnl_handler.h"
#include "shill/service.h"
#include "shill/shill_event.h"

using std::string;
using std::vector;

namespace shill {
Device::Device(ControlInterface *control_interface,
               EventDispatcher *dispatcher,
               Manager *manager,
               const string &link_name,
               int interface_index)
    : powered_(true),
      reconnect_(true),
      manager_(manager),
      link_name_(link_name),
      adaptor_(control_interface->CreateDeviceAdaptor(this)),
      interface_index_(interface_index),
      running_(false) {

  store_.RegisterConstString(flimflam::kAddressProperty, &hardware_address_);
  HelpRegisterDerivedString(flimflam::kDBusConnectionProperty,
                            &Device::GetRpcConnectionIdentifier,
                            NULL);
  HelpRegisterDerivedString(flimflam::kDBusObjectProperty,
                            &Device::GetRpcIdentifier,
                            NULL);
  // TODO(cmasone): Chrome doesn't use this...does anyone?
  // store_.RegisterConstString(flimflam::kInterfaceProperty, &link_name_);
  HelpRegisterDerivedStrings(flimflam::kIPConfigsProperty,
                             &Device::AvailableIPConfigs,
                             NULL);
  store_.RegisterConstString(flimflam::kNameProperty, &link_name_);
  store_.RegisterBool(flimflam::kPoweredProperty, &powered_);
  // TODO(cmasone): Chrome doesn't use this...does anyone?
  // store_.RegisterConstBool(flimflam::kReconnectProperty, &reconnect_);

  // TODO(cmasone): Figure out what shill concept maps to flimflam's "Network".
  // known_properties_.push_back(flimflam::kNetworksProperty);

  // Initialize Interface monitor, so we can detect new interfaces
  VLOG(2) << "Device " << link_name_ << " index " << interface_index;
}

Device::~Device() {
  VLOG(2) << "Device " << link_name_ << " destroyed.";
}

void Device::Start() {
  running_ = true;
  VLOG(2) << "Device " << link_name_ << " starting.";
  adaptor_->UpdateEnabled();
}

void Device::Stop() {
  running_ = false;
  adaptor_->UpdateEnabled();
}

bool Device::TechnologyIs(const Technology type) {
  return false;
}

void Device::LinkEvent(unsigned flags, unsigned change) {
  VLOG(2) << "Device " << link_name_ << " flags " << flags << " changed "
          << change;
}

void Device::Scan() {
  VLOG(2) << "Device " << link_name_ << " scan requested.";
}

string Device::GetRpcIdentifier() {
  return adaptor_->GetRpcIdentifier();
}

const string& Device::UniqueName() const {
  // TODO(pstew): link_name is only run-time unique and won't persist
  return link_name();
}

void Device::DestroyIPConfig() {
  if (ipconfig_.get()) {
    RTNLHandler::GetInstance()->RemoveInterfaceAddress(interface_index_,
                                                       *ipconfig_);
    ipconfig_->ReleaseIP();
    ipconfig_ = NULL;
  }
}

bool Device::AcquireDHCPConfig() {
  DestroyIPConfig();
  ipconfig_ = DHCPProvider::GetInstance()->CreateConfig(link_name());
  ipconfig_->RegisterUpdateCallback(
      NewCallback(this, &Device::IPConfigUpdatedCallback));
  return ipconfig_->RequestIP();
}

void Device::HelpRegisterDerivedString(const string &name,
                                    string(Device::*get)(void),
                                    bool(Device::*set)(const string&)) {
  store_.RegisterDerivedString(
      name,
      StringAccessor(new CustomAccessor<Device, string>(this, get, set)));
}

void Device::HelpRegisterDerivedStrings(const string &name,
                                     Strings(Device::*get)(void),
                                     bool(Device::*set)(const Strings&)) {
  store_.RegisterDerivedStrings(
      name,
      StringsAccessor(new CustomAccessor<Device, Strings>(this, get, set)));
}

void Device::IPConfigUpdatedCallback(const IPConfigRefPtr &ipconfig,
                                     bool success) {
  // TODO(petkov): Use DeviceInfo to configure IP, etc. -- maybe through
  // ConfigIP? Also, maybe allow forwarding the callback to interested listeners
  // (e.g., the Manager).
  if (success) {
      RTNLHandler::GetInstance()->AddInterfaceAddress(interface_index_,
                                                      *ipconfig);
  }
}

vector<string> Device::AvailableIPConfigs() {
  string id = (ipconfig_.get() ? ipconfig_->GetRpcIdentifier() : string());
  return vector<string>(1, id);
}

string Device::GetRpcConnectionIdentifier() {
  return adaptor_->GetRpcConnectionIdentifier();
}

}  // namespace shill
