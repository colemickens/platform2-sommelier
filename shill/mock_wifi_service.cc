// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_wifi_service.h"

using std::string;
using std::vector;

namespace shill {

class ControlInterface;
class EventDispatcher;
class Manager;

MockWiFiService::MockWiFiService(ControlInterface *control_interface,
                                 EventDispatcher *dispatcher,
                                 Manager *manager,
                                 const WiFiRefPtr &device,
                                 const vector<uint8_t> &ssid,
                                 const string &mode,
                                 const string &security,
                                 bool hidden_ssid)
    : WiFiService(
        control_interface, dispatcher, manager, device, ssid, mode, security,
        hidden_ssid) {}

MockWiFiService::~MockWiFiService() {}

}  // namespace shill
