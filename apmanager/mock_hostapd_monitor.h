// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_MOCK_HOSTAPD_MONITOR_H_
#define APMANAGER_MOCK_HOSTAPD_MONITOR_H_

#include <gmock/gmock.h>

#include "apmanager/hostapd_monitor.h"

namespace apmanager {

class MockHostapdMonitor : public HostapdMonitor {
 public:
  MockHostapdMonitor();
  ~MockHostapdMonitor() override;

  MOCK_METHOD0(Start, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockHostapdMonitor);
};

}  // namespace apmanager

#endif  // APMANAGER_MOCK_HOSTAPD_MONITOR_H_
