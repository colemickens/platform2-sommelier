// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/ethernet/mock_ethernet.h"

#include <string>

namespace shill {

using std::string;

MockEthernet::MockEthernet(ControlInterface* control_interface,
                           EventDispatcher* dispatcher,
                           Metrics* metrics,
                           Manager* manager,
                           const string& link_name,
                           const string& address,
                           int interface_index)
    : Ethernet(control_interface,
               dispatcher,
               metrics,
               manager,
               link_name,
               address,
               interface_index) {}

MockEthernet::~MockEthernet() = default;

}  // namespace shill
