// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_MOCK_MODEM_MANAGER_PROXY_H_
#define SHILL_CELLULAR_MOCK_MODEM_MANAGER_PROXY_H_

#include <string>
#include <vector>

#include <base/macros.h>
#include <gmock/gmock.h>

#include "shill/cellular/modem_manager_proxy_interface.h"

namespace shill {

class MockModemManagerProxy : public ModemManagerProxyInterface {
 public:
  MockModemManagerProxy();
  ~MockModemManagerProxy() override;

  MOCK_METHOD0(EnumerateDevices, std::vector<std::string>());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockModemManagerProxy);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_MOCK_MODEM_MANAGER_PROXY_H_
