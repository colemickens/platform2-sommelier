// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/virtual_device.h"

#include <gtest/gtest.h>

#include "shill/event_dispatcher.h"
#include "shill/mock_glib.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/nice_mock_control.h"
#include "shill/technology.h"

namespace shill {

namespace {
const char kTestDeviceName[] = "tun0";
const int kTestInterfaceIndex = 5;
}  // namespace {}

class VirtualDeviceTest : public testing::Test {
 public:
  VirtualDeviceTest()
      : metrics_(&dispatcher_),
        manager_(&control_, &dispatcher_, &metrics_, &glib_),
        device_(new VirtualDevice(&control_,
                                  &dispatcher_,
                                  &metrics_,
                                  &manager_,
                                  kTestDeviceName,
                                  kTestInterfaceIndex,
                                  Technology::kVPN)) {}

  virtual ~VirtualDeviceTest() {}

 protected:
  NiceMockControl control_;
  EventDispatcher dispatcher_;
  MockMetrics metrics_;
  MockGLib glib_;
  MockManager manager_;

  VirtualDeviceRefPtr device_;
};

TEST_F(VirtualDeviceTest, technology) {
  EXPECT_EQ(Technology::kVPN, device_->technology());
  EXPECT_NE(Technology::kEthernet, device_->technology());
}

}  // namespace shill
