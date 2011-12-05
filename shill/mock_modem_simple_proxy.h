// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_MODEM_SIMPLE_PROXY_H_
#define SHILL_MOCK_MODEM_SIMPLE_PROXY_H_

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/modem_simple_proxy_interface.h"

namespace shill {

class MockModemSimpleProxy : public ModemSimpleProxyInterface {
 public:
  MockModemSimpleProxy();
  virtual ~MockModemSimpleProxy();

  MOCK_METHOD2(GetModemStatus, void(AsyncCallHandler *call_handler,
                                    int timeout));
  MOCK_METHOD3(Connect, void(const DBusPropertiesMap &properties,
                             AsyncCallHandler *call_handler, int timeout));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockModemSimpleProxy);
};

}  // namespace shill

#endif  // SHILL_MOCK_MODEM_SIMPLE_PROXY_H_
