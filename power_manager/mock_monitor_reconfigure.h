// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_MOCK_MONITOR_RECONFIGURE_H_
#define POWER_MANAGER_MOCK_MONITOR_RECONFIGURE_H_

#include <gmock/gmock.h>

#include "power_manager/monitor_reconfigure.h"

namespace power_manager {

class MockMonitorReconfigure : public MonitorReconfigure{
 public:
  MOCK_CONST_METHOD0(is_projecting, bool());
};

}  // namespace power_manager

#endif // POWER_MANAGER_MOCK_MONITOR_RECONFIGURE_H_
