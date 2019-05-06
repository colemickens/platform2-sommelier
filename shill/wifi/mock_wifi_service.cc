// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wifi/mock_wifi_service.h"

using std::string;
using std::vector;

namespace shill {

class ControlInterface;
class EventDispatcher;
class Manager;

MockWiFiService::MockWiFiService(ControlInterface* control_interface,
                                 EventDispatcher* dispatcher,
                                 Metrics* metrics,
                                 Manager* manager,
                                 WiFiProvider* provider,
                                 const vector<uint8_t>& ssid,
                                 const string& mode,
                                 const string& security,
                                 bool hidden_ssid)
    : WiFiService(
        control_interface, dispatcher, metrics, manager, provider, ssid, mode,
        security, hidden_ssid) {
  ON_CALL(*this, GetSupplicantConfigurationParameters())
      .WillByDefault(testing::Return(KeyValueStore()));
}

MockWiFiService::~MockWiFiService() = default;

}  // namespace shill
