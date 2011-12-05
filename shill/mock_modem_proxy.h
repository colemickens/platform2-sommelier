// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_MODEM_PROXY_H_
#define SHILL_MOCK_MODEM_PROXY_H_

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/modem_proxy_interface.h"

namespace shill {

class AsyncCallHandler;

class MockModemProxy : public ModemProxyInterface {
 public:
  MockModemProxy();
  virtual ~MockModemProxy();

  MOCK_METHOD3(Enable, void(bool enable, AsyncCallHandler *call_handler,
                            int timeout));
  MOCK_METHOD1(Enable, void(bool enable));
  MOCK_METHOD0(Disconnect, void());
  MOCK_METHOD2(GetModemInfo, void(AsyncCallHandler *call_handler, int timeout));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockModemProxy);
};

}  // namespace shill

#endif  // SHILL_MOCK_MODEM_PROXY_H_
