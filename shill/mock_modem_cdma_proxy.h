// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_MODEM_CDMA_PROXY_H_
#define SHILL_MOCK_MODEM_CDMA_PROXY_H_

#include <string>

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/modem_cdma_proxy_interface.h"

namespace shill {

class MockModemCDMAProxy : public ModemCDMAProxyInterface {
 public:
  MockModemCDMAProxy();
  virtual ~MockModemCDMAProxy();

  MOCK_METHOD1(Activate, uint32(const std::string &carrier));
  MOCK_METHOD2(GetRegistrationState, void(uint32 *cdma_1x_state,
                                          uint32 *evdo_state));
  MOCK_METHOD0(GetSignalQuality, uint32());
  MOCK_METHOD0(MEID, const std::string());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockModemCDMAProxy);
};

}  // namespace shill

#endif  // SHILL_MOCK_MODEM_CDMA_PROXY_H_
