// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_DEVICE_
#define SHILL_MOCK_DEVICE_

#include <base/memory/ref_counted.h>
#include <gmock/gmock.h>

#include "shill/device.h"

namespace shill {

class ControlInterface;
class EventDispatcher;

using ::testing::_;
using ::testing::Return;

class MockDevice : public Device {
 public:
  // A constructor for the Device object
  MockDevice(ControlInterface *control_interface,
             EventDispatcher *dispatcher,
             const string &link_name,
             int interface_index)
      : Device(control_interface, dispatcher, link_name, interface_index) {
    ON_CALL(*this, TechnologyIs(_)).WillByDefault(Return(false));
  }
  virtual ~MockDevice() {}

  MOCK_METHOD0(Start, void(void));
  MOCK_METHOD0(Stop, void(void));
  MOCK_METHOD1(TechnologyIs, bool(Technology));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDevice);
};

}  // namespace shill

#endif  // SHILL_MOCK_DEVICE_
