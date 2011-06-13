// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular.h"

#include <string>

#include <base/logging.h>

#include "shill/cellular_service.h"
#include "shill/control_interface.h"
#include "shill/device.h"
#include "shill/device_info.h"
#include "shill/manager.h"
#include "shill/shill_event.h"

using std::string;

namespace shill {

Cellular::Cellular(ControlInterface *control_interface,
                   EventDispatcher *dispatcher,
                   Manager *manager,
                   const string &link,
                   int interface_index)
    : Device(control_interface,
             dispatcher,
             manager,
             link,
             interface_index),
      service_(new CellularService(control_interface,
                                   dispatcher,
                                   this,
                                   "service-" + link)),
      service_registered_(false) {
  VLOG(2) << "Cellular device " << link_name() << " initialized.";
}

Cellular::~Cellular() {
  Stop();
}

void Cellular::Start() {
  Device::Start();
}

void Cellular::Stop() {
  manager_->DeregisterService(service_.get());
  Device::Stop();
}

bool Cellular::TechnologyIs(const Device::Technology type) {
  return type == Device::kCellular;
}

}  // namespace shill
