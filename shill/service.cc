// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <time.h>
#include <stdio.h>

#include <map>
#include <string>
#include <vector>

#include <base/logging.h>

#include "shill/control_interface.h"
#include "shill/device_config_interface.h"
#include "shill/error.h"
#include "shill/service.h"
#include "shill/service_dbus_adaptor.h"

using std::map;
using std::string;
using std::vector;

namespace shill {
Service::Service(ControlInterface *control_interface,
                 EventDispatcher */* dispatcher */,
                 DeviceConfigInterfaceRefPtr config_interface,
                 const string& name)
    : name_(name),
      available_(false),
      configured_(false),
      auto_connect_(false),
      configuration_(NULL),
      connection_(NULL),
      config_interface_(config_interface),
      adaptor_(control_interface->CreateServiceAdaptor(this)) {
  // Initialize Interface montior, so we can detect new interfaces
  VLOG(2) << "Service initialized.";
}

Service::~Service() {}

bool Service::SetBoolProperty(const string& name, bool value, Error *error) {
  VLOG(2) << "Setting " << name << " as a bool.";
  // TODO(cmasone): Set actual properties.
  return true;
}

bool Service::SetInt32Property(const std::string& name,
                              int32 value,
                              Error *error) {
  VLOG(2) << "Setting " << name << " as an int32.";
  // TODO(cmasone): Set actual properties.
  return true;
}

bool Service::SetStringProperty(const string& name,
                                const string& value,
                                Error *error) {
  VLOG(2) << "Setting " << name << " as a string.";
  // TODO(cmasone): Set actual properties.
  return true;
}

bool Service::SetStringmapProperty(const string& name,
                                   const std::map<string, string>& values,
                                   Error *error) {
  VLOG(2) << "Setting " << name << " as a map of string:string";
  // TODO(cmasone): Set actual properties.
  return true;
}

bool Service::SetUint8Property(const std::string& name,
                               uint8 value,
                               Error *error) {
  VLOG(2) << "Setting " << name << " as a uint8.";
  // TODO(cmasone): Set actual properties.
  return true;
}

}  // namespace shill
