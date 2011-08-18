// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_device.h"

#include <string>

#include <base/memory/ref_counted.h>
#include <base/stringprintf.h>
#include <gmock/gmock.h>

namespace shill {

class ControlInterface;
class EventDispatcher;

using ::testing::_;
using ::testing::Return;
using std::string;

MockDevice::MockDevice(ControlInterface *control_interface,
                       EventDispatcher *dispatcher,
                       Manager *manager,
                       const std::string &link_name,
                       const std::string &address,
                       int interface_index)
    : Device(control_interface,
             dispatcher,
             manager,
             link_name,
             address,
             interface_index) {
  ON_CALL(*this, TechnologyIs(_)).WillByDefault(Return(false));
}

MockDevice::~MockDevice() {}

}  // namespace shill
