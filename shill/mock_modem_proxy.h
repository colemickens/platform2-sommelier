// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_MODEM_PROXY_H_
#define SHILL_MOCK_MODEM_PROXY_H_

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/modem_proxy_interface.h"

namespace shill {

class MockModemProxy : public ModemProxyInterface {
 public:
  MockModemProxy();
  virtual ~MockModemProxy();

  MOCK_METHOD4(Enable, void(bool enable, Error *error,
                            const ResultCallback &callback, int timeout));
  MOCK_METHOD3(Disconnect, void(Error *error, const ResultCallback &callback,
                                int timeout));
  MOCK_METHOD3(GetModemInfo, void(Error *error,
                                  const ModemInfoCallback &callback,
                                  int timeout));
  MOCK_METHOD1(set_state_changed_callback,
               void(const ModemStateChangedSignalCallback &callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockModemProxy);
};

}  // namespace shill

#endif  // SHILL_MOCK_MODEM_PROXY_H_
