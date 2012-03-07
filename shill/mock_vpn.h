// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_VPN_
#define SHILL_MOCK_VPN_

#include <gmock/gmock.h>

#include "shill/vpn.h"

namespace shill {

class MockVPN : public VPN {
 public:
  MockVPN(ControlInterface *control,
          EventDispatcher *dispatcher,
          Metrics *metrics,
          Manager *manager,
          const std::string &link_name,
          int interface_index);
  virtual ~MockVPN();

  MOCK_METHOD0(Stop, void());
  MOCK_METHOD1(UpdateIPConfig, void(const IPConfig::Properties &properties));
  MOCK_METHOD0(OnDisconnected, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVPN);
};

}  // namespace shill

#endif  // SHILL_MOCK_VPN_
