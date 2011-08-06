// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_MODEM_SIMPLE_PROXY_H_
#define SHILL_MOCK_MODEM_SIMPLE_PROXY_H_

#include <gmock/gmock.h>

#include "shill/modem_simple_proxy_interface.h"

namespace shill {

class MockModemSimpleProxy : public ModemSimpleProxyInterface {
 public:
  MOCK_METHOD0(GetStatus, DBusPropertiesMap());
  MOCK_METHOD1(Connect, void(const DBusPropertiesMap &properties));
};

}  // namespace shill

#endif  // SHILL_MOCK_MODEM_SIMPLE_PROXY_H_
