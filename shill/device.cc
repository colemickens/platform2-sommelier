// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <time.h>
#include <stdio.h>

#include <string>
#include <vector>

#include <base/logging.h>
#include <base/memory/ref_counted.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/control_interface.h"
#include "shill/device.h"
#include "shill/device_dbus_adaptor.h"
#include "shill/dhcp_provider.h"
#include "shill/error.h"
#include "shill/manager.h"
#include "shill/shill_event.h"

using std::string;
using std::vector;

namespace shill {
Device::Device(ControlInterface *control_interface,
               EventDispatcher *dispatcher,
               Manager *manager,
               const string &link_name,
               int interface_index)
    : manager_(manager),
      link_name_(link_name),
      adaptor_(control_interface->CreateDeviceAdaptor(this)),
      interface_index_(interface_index),
      running_(false) {
  known_properties_.push_back(flimflam::kNameProperty);
  known_properties_.push_back(flimflam::kTypeProperty);
  known_properties_.push_back(flimflam::kPoweredProperty);
  known_properties_.push_back(flimflam::kScanningProperty);
  // known_properties_.push_back(flimflam::kReconnectProperty);
  known_properties_.push_back(flimflam::kScanIntervalProperty);
  known_properties_.push_back(flimflam::kBgscanMethodProperty);
  known_properties_.push_back(flimflam::kBgscanShortIntervalProperty);
  known_properties_.push_back(flimflam::kBgscanSignalThresholdProperty);
  known_properties_.push_back(flimflam::kNetworksProperty);
  known_properties_.push_back(flimflam::kIPConfigsProperty);
  known_properties_.push_back(flimflam::kCellularAllowRoamingProperty);
  known_properties_.push_back(flimflam::kCarrierProperty);
  known_properties_.push_back(flimflam::kMeidProperty);
  known_properties_.push_back(flimflam::kImeiProperty);
  known_properties_.push_back(flimflam::kImsiProperty);
  known_properties_.push_back(flimflam::kEsnProperty);
  known_properties_.push_back(flimflam::kMdnProperty);
  known_properties_.push_back(flimflam::kModelIDProperty);
  known_properties_.push_back(flimflam::kManufacturerProperty);
  known_properties_.push_back(flimflam::kFirmwareRevisionProperty);
  known_properties_.push_back(flimflam::kHardwareRevisionProperty);
  known_properties_.push_back(flimflam::kPRLVersionProperty);
  known_properties_.push_back(flimflam::kSIMLockStatusProperty);
  known_properties_.push_back(flimflam::kFoundNetworksProperty);
  known_properties_.push_back(flimflam::kDBusConnectionProperty);
  known_properties_.push_back(flimflam::kDBusObjectProperty);
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

bool Device::Contains(const std::string &property) {
  vector<string>::iterator it;
  for (it = known_properties_.begin(); it != known_properties_.end(); ++it) {
    if (property == *it)
      return true;
  }
  return false;
}

bool Device::SetBoolProperty(const string& name, bool value, Error *error) {
  VLOG(2) << "Setting " << name << " as a bool.";
  // TODO(cmasone): Set actual properties.
  return true;
}

bool Device::SetInt32Property(const std::string& name,
                              int32 value,
                              Error *error) {
  VLOG(2) << "Setting " << name << " as an int32.";
  // TODO(cmasone): Set actual properties.
  return true;
}

bool Device::SetUint16Property(const std::string& name,
                               uint16 value,
                               Error *error) {
  VLOG(2) << "Setting " << name << " as a uint16.";
  // TODO(cmasone): Set actual properties.
  return true;
}

const string& Device::UniqueName() const {
  // TODO(pstew): link_name is only run-time unique and won't persist
  return link_name();
}

void Device::DestroyIPConfig() {
  if (ipconfig_.get()) {
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

void Device::IPConfigUpdatedCallback(IPConfigRefPtr ipconfig, bool success) {
  // TODO(petkov): Use DeviceInfo to configure IP, etc. -- maybe through
  // ConfigIP? Also, maybe allow forwarding the callback to interested listeners
  // (e.g., the Manager).
}

}  // namespace shill
