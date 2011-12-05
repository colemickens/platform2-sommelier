// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_MODEM_GSM_NETWORK_PROXY_H_
#define SHILL_MOCK_MODEM_GSM_NETWORK_PROXY_H_

#include <string>

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/modem_gsm_network_proxy_interface.h"

namespace shill {

class MockModemGSMNetworkProxy : public ModemGSMNetworkProxyInterface {
 public:
  MockModemGSMNetworkProxy();
  virtual ~MockModemGSMNetworkProxy();

  MOCK_METHOD2(GetRegistrationInfo, void(AsyncCallHandler *call_handler,
                                         int timeout));
  MOCK_METHOD0(GetSignalQuality, uint32());
  MOCK_METHOD3(Register, void(const std::string &network_id,
                              AsyncCallHandler *call_handler, int timeout));
  MOCK_METHOD2(Scan, void(AsyncCallHandler *call_handler, int timeout));
  MOCK_METHOD0(AccessTechnology, uint32());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockModemGSMNetworkProxy);
};

}  // namespace shill

#endif  // SHILL_MOCK_MODEM_GSM_NETWORK_PROXY_H_
