// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <time.h>
#include <stdio.h>

#include <string>

#include <base/logging.h>

#include "shill/control_interface.h"
#include "shill/device.h"
#include "shill/shill_event.h"

#include "shill/wifi.h"

namespace shill {
WiFi::WiFi(ControlInterface *control_interface,
           EventDispatcher *dispatcher,
           Manager *manager,
           const std::string& link_name,
           int interface_index)
    : Device(control_interface, dispatcher, manager, link_name,
             interface_index) {
  VLOG(2) << "WiFi device " << link_name << " initialized.";
}

WiFi::~WiFi() {
}

bool WiFi::TechnologyIs(const Device::Technology type) {
  return type == Device::kWifi;
}

}  // namespace shill
