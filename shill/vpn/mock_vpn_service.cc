// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/vpn/mock_vpn_service.h"

namespace shill {

MockVPNService::MockVPNService(ControlInterface* control,
                               EventDispatcher* dispatcher,
                               Metrics* metrics,
                               Manager* manager,
                               VPNDriver* driver)
    : VPNService(control, dispatcher, metrics, manager, driver) {}

MockVPNService::~MockVPNService() = default;

}  // namespace shill
