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
#include <base/stl_util-inl.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/control_interface.h"
#include "shill/device_dbus_adaptor.h"
#include "shill/dhcp_config.h"
#include "shill/dhcp_provider.h"
#include "shill/error.h"
#include "shill/manager.h"
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

  RegisterConstString(flimflam::kAddressProperty, &hardware_address_);
  // TODO(cmasone): Chrome doesn't use this...does anyone?
  // RegisterConstString(flimflam::kInterfaceProperty, &link_name_);
  RegisterConstString(flimflam::kNameProperty, &link_name_);
  RegisterBool(flimflam::kPoweredProperty, &powered_);
  // TODO(cmasone): Chrome doesn't use this...does anyone?
  // RegisterConstBool(flimflam::kReconnectProperty, &reconnect_);

  // TODO(cmasone): Add support for R/O properties that return DBus object paths
  // known_properties_.push_back(flimflam::kDBusConnectionProperty);
  // known_properties_.push_back(flimflam::kDBusObjectProperty);
  // known_properties_.push_back(flimflam::kIPConfigsProperty);
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

bool Device::SetBoolProperty(const string& name, bool value, Error *error) {
  VLOG(2) << "Setting " << name << " as a bool.";
  bool set = (ContainsKey(bool_properties_, name) &&
              bool_properties_[name]->Set(value));
  if (!set && error)
    error->Populate(Error::kInvalidArguments, name + " is not a R/W bool.");
  return set;
}

bool Device::SetInt32Property(const std::string& name,
                              int32 value,
                              Error *error) {
  VLOG(2) << "Setting " << name << " as an int32.";
  bool set = (ContainsKey(int32_properties_, name) &&
              int32_properties_[name]->Set(value));
  if (!set && error)
    error->Populate(Error::kInvalidArguments, name + " is not a R/W int32.");
  return set;
}

bool Device::SetUint16Property(const std::string& name,
                               uint16 value,
                               Error *error) {
  VLOG(2) << "Setting " << name << " as a uint16.";
  bool set = (ContainsKey(uint16_properties_, name) &&
              uint16_properties_[name]->Set(value));
  if (!set && error)
    error->Populate(Error::kInvalidArguments, name + " is not a R/W uint16.");
  return set;
}

bool Device::SetStringProperty(const string& name,
                               const string& value,
                               Error *error) {
  VLOG(2) << "Setting " << name << " as a string.";
  bool set = (ContainsKey(string_properties_, name) &&
              string_properties_[name]->Set(value));
  if (!set && error)
    error->Populate(Error::kInvalidArguments, name + " is not a R/W string.");
  return set;
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

}  // namespace shill
