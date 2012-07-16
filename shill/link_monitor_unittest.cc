// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/link_monitor.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/mock_connection.h"
#include "shill/mock_control.h"
#include "shill/mock_device_info.h"
#include "shill/mock_event_dispatcher.h"
#include "shill/mock_glib.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/mock_sockets.h"

using testing::StrictMock;
using testing::Test;

namespace shill {

class LinkMonitorTest : public Test {
 public:
  LinkMonitorTest()
      : manager_(&control_interface_, &dispatcher_, &metrics_, &glib_),
        device_info_(&control_interface_, &dispatcher_, &metrics_, &manager_),
        connection_(new StrictMock<MockConnection>(&device_info_)),
        link_monitor_(connection_.get(),
                      &dispatcher_,
                      base::Bind(&LinkMonitorTest::OnLinkFail,
                                 base::Unretained(this))) {}
  virtual ~LinkMonitorTest() {}

  MOCK_METHOD0(OnLinkFail, void());

 protected:
  MockGLib glib_;
  MockControl control_interface_;
  StrictMock<MockEventDispatcher> dispatcher_;
  MockMetrics metrics_;
  MockManager manager_;
  MockDeviceInfo device_info_;
  scoped_refptr<MockConnection> connection_;
  LinkMonitor link_monitor_;
};

TEST_F(LinkMonitorTest, Constructor) {
}

}  // namespace shill
