// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_MOCK_MODEM_PROXY_H_
#define SHILL_CELLULAR_MOCK_MODEM_PROXY_H_

#include <base/macros.h>
#include <gmock/gmock.h>

#include "shill/cellular/modem_proxy_interface.h"

namespace shill {

class MockModemProxy : public ModemProxyInterface {
 public:
  MockModemProxy();
  ~MockModemProxy() override;

  MOCK_METHOD4(Enable, void(bool enable, Error* error,
                            const ResultCallback& callback, int timeout));
  MOCK_METHOD3(Disconnect, void(Error* error, const ResultCallback& callback,
                                int timeout));
  MOCK_METHOD3(GetModemInfo, void(Error* error,
                                  const ModemInfoCallback& callback,
                                  int timeout));
  MOCK_METHOD1(set_state_changed_callback,
               void(const ModemStateChangedSignalCallback& callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockModemProxy);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_MOCK_MODEM_PROXY_H_
