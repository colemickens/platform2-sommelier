// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/vpn_service.h"

#include <base/logging.h>

#include "shill/technology.h"
#include "shill/vpn_driver.h"

using std::string;

namespace shill {

VPNService::VPNService(ControlInterface *control,
                       EventDispatcher *dispatcher,
                       Metrics *metrics,
                       Manager *manager,
                       VPNDriver *driver)
    : Service(control, dispatcher, metrics, manager, Technology::kVPN),
      driver_(driver) {}

VPNService::~VPNService() {}

void VPNService::Connect(Error *error) {
  driver_->Connect(this, error);
}

string VPNService::GetStorageIdentifier() const {
  NOTIMPLEMENTED();
  return "";
}

string VPNService::GetDeviceRpcId(Error *error) {
  NOTIMPLEMENTED();
  error->Populate(Error::kNotSupported);
  return "/";
}

}  // namespace shill
