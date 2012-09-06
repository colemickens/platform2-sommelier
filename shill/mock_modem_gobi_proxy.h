// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_MODEM_GOBI_PROXY_H_
#define SHILL_MOCK_MODEM_GOBI_PROXY_H_

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/modem_gobi_proxy_interface.h"

namespace shill {

class MockModemGobiProxy : public ModemGobiProxyInterface {
 public:
  MockModemGobiProxy();
  virtual ~MockModemGobiProxy();

  MOCK_METHOD4(SetCarrier, void(const std::string &carrier,
                                Error *error, const ResultCallback &callback,
                                int timeout));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockModemGobiProxy);
};

}  // namespace shill

#endif  // SHILL_MOCK_MODEM_GOBI_PROXY_H_
