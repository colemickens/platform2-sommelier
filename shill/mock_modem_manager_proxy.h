// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_MODEM_MANAGER_PROXY_H_
#define SHILL_MOCK_MODEM_MANAGER_PROXY_H_

#include <gmock/gmock.h>

#include "shill/modem_manager_proxy_interface.h"

namespace shill {

class MockModemManagerProxy : public ModemManagerProxyInterface {
 public:
  MOCK_METHOD0(EnumerateDevices, std::vector<DBus::Path>());
};

}  // namespace shill

#endif  // SHILL_MOCK_MODEM_MANAGER_PROXY_H_
