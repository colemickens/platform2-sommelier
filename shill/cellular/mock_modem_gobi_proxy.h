// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_MOCK_MODEM_GOBI_PROXY_H_
#define SHILL_CELLULAR_MOCK_MODEM_GOBI_PROXY_H_

#include <string>

#include <base/macros.h>
#include <gmock/gmock.h>

#include "shill/cellular/modem_gobi_proxy_interface.h"

namespace shill {

class MockModemGobiProxy : public ModemGobiProxyInterface {
 public:
  MockModemGobiProxy();
  ~MockModemGobiProxy() override;

  MOCK_METHOD4(SetCarrier, void(const std::string& carrier,
                                Error* error, const ResultCallback& callback,
                                int timeout));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockModemGobiProxy);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_MOCK_MODEM_GOBI_PROXY_H_
