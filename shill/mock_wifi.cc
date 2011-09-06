// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_wifi.h"

#include <string>

namespace shill {

using std::string;

MockWiFi::MockWiFi(ControlInterface *control_interface,
                   EventDispatcher *dispatcher,
                   Manager *manager,
                   const string &link_name,
                   const string &address,
                   int interface_index)
    : WiFi(control_interface,
           dispatcher,
           manager,
           link_name,
           address,
           interface_index) {}

MockWiFi::~MockWiFi() {}

}  // namespace shill
