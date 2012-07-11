// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_manager.h"

#include <gmock/gmock.h>

using testing::Return;

namespace shill {

MockManager::MockManager(ControlInterface *control_interface,
                         EventDispatcher *dispatcher,
                         Metrics *metrics,
                         GLib *glib)
    : Manager(control_interface, dispatcher, metrics, glib, "", "", "") {
  EXPECT_CALL(*this, device_info())
      .WillRepeatedly(Return(reinterpret_cast<DeviceInfo *>(NULL)));
}

MockManager::~MockManager() {}

}  // namespace shill
