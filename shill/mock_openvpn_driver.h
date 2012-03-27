// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_OPENVPN_DRIVER_
#define SHILL_MOCK_OPENVPN_DRIVER_

#include <gmock/gmock.h>

#include "shill/openvpn_driver.h"

namespace shill {

class MockOpenVPNDriver : public OpenVPNDriver {
 public:
  MockOpenVPNDriver(const KeyValueStore &args);
  virtual ~MockOpenVPNDriver();

  MOCK_METHOD0(OnReconnecting, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockOpenVPNDriver);
};

}  // namespace shill

#endif  // SHILL_MOCK_OPENVPN_DRIVER_
