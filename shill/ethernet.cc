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

#include "shill/ethernet.h"

using std::string;

namespace shill {
Ethernet::Ethernet(ControlInterface *control_interface,
                   EventDispatcher *dispatcher,
                   const string &link_name,
                   int interface_index)
  : Device(control_interface, dispatcher, link_name, interface_index) {
  VLOG(2) << "Ethernet device " << link_name << " initialized.";
}

Ethernet::~Ethernet() {
}

bool Ethernet::TechnologyIs(const Device::Technology type) {
  return type == Device::kEthernet;
}

}  // namespace shill
