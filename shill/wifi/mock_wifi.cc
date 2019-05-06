// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wifi/mock_wifi.h"

#include <memory>
#include <string>

namespace shill {

using std::string;

MockWiFi::MockWiFi(ControlInterface* control_interface,
                   EventDispatcher* dispatcher,
                   Metrics* metrics,
                   Manager* manager,
                   const string& link_name,
                   const string& address,
                   int interface_index,
                   WakeOnWiFiInterface* wake_on_wifi)
    : WiFi(control_interface,
           dispatcher,
           metrics,
           manager,
           link_name,
           address,
           interface_index,
           std::unique_ptr<WakeOnWiFiInterface>(wake_on_wifi)) {}

MockWiFi::~MockWiFi() = default;

}  // namespace shill
