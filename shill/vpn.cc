// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/vpn.h"

using std::string;

namespace shill {

VPN::VPN(ControlInterface *control,
         EventDispatcher *dispatcher,
         Metrics *metrics,
         Manager *manager,
         const string &link_name,
         int interface_index)
    : Device(control, dispatcher, metrics, manager, link_name, "",
             interface_index, Technology::kVPN) {}

VPN::~VPN() {}

bool VPN::TechnologyIs(const Technology::Identifier type) const {
  return type == Technology::kVPN;
}

}  // namespace shill
