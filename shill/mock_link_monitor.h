// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_LINK_MONITOR_H_
#define SHILL_MOCK_LINK_MONITOR_H_

#include "shill/link_monitor.h"

#include <gmock/gmock.h>

namespace shill {

class MockLinkMonitor : public LinkMonitor {
 public:
  MockLinkMonitor();
  virtual ~MockLinkMonitor();

  MOCK_METHOD0(Start, bool());
  MOCK_METHOD0(Stop, void());
  MOCK_CONST_METHOD0(GetResponseTimeMilliseconds, int());
  MOCK_CONST_METHOD0(IsGatewayFound, bool());

  DISALLOW_COPY_AND_ASSIGN(MockLinkMonitor);
};

}  // namespace shill

#endif  // SHILL_MOCK_LINK_MONITOR_H_
