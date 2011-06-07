// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <time.h>
#include <stdio.h>

#include <string>

#include <base/logging.h>

#include "shill/control_interface.h"
#include "shill/device_config_interface.h"
#include "shill/service.h"
#include "shill/service_dbus_adaptor.h"

using std::string;

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

}  // namespace shill
