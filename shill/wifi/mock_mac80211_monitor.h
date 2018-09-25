// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_WIFI_MOCK_MAC80211_MONITOR_H_
#define SHILL_WIFI_MOCK_MAC80211_MONITOR_H_

#include <string>

#include <gmock/gmock.h>

#include "shill/wifi/mac80211_monitor.h"

namespace shill {

class MockMac80211Monitor : public Mac80211Monitor {
 public:
  MockMac80211Monitor(EventDispatcher* dispatcher,
                      const std::string& link_name,
                      size_t queue_length_limit,
                      const base::Closure& on_repair_callback,
                      Metrics* metrics);
  ~MockMac80211Monitor() override;

  MOCK_METHOD1(Start, void(const std::string& phy_name));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD1(UpdateConnectedState, void(bool new_state));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockMac80211Monitor);
};

}  // namespace shill

#endif  // SHILL_WIFI_MOCK_MAC80211_MONITOR_H_
