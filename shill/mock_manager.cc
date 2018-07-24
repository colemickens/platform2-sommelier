// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_manager.h"

#include <vector>

#include <gmock/gmock.h>

using std::string;
using std::vector;
using testing::_;
using testing::Invoke;
using testing::Return;

namespace shill {

MockManager::MockManager(ControlInterface* control_interface,
                         EventDispatcher* dispatcher,
                         Metrics* metrics,
                         GLib* glib)
    : Manager(control_interface, dispatcher, metrics, glib, "", "", ""),
      mock_device_info_(nullptr) {
  EXPECT_CALL(*this, device_info())
      .WillRepeatedly(Invoke(this, &MockManager::mock_device_info));
  ON_CALL(*this, FilterPrependDNSServersByFamily(_))
      .WillByDefault(Return(vector<string>()));
}

MockManager::~MockManager() {}

}  // namespace shill
