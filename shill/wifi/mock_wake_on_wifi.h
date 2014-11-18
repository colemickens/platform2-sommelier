// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_WIFI_MOCK_WAKE_ON_WIFI_H_
#define SHILL_WIFI_MOCK_WAKE_ON_WIFI_H_

#include <string>

#include <gmock/gmock.h>

#include "shill/wifi/wake_on_wifi.h"

namespace shill {

class MockWakeOnWiFi : public WakeOnWiFi {
 public:
  MockWakeOnWiFi(NetlinkManager *netlink_manager, EventDispatcher *dispatcher,
                 Metrics *metrics);
  ~MockWakeOnWiFi() override;

  MOCK_METHOD0(OnAfterResume, void());
  MOCK_METHOD1(OnBeforeSuspend, void(const ResultCallback &callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWakeOnWiFi);
};

}  // namespace shill

#endif  // SHILL_WIFI_MOCK_WAKE_ON_WIFI_H_
