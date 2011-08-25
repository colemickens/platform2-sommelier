// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_MODEM_GSM_NETWORK_PROXY_H_
#define SHILL_MOCK_MODEM_GSM_NETWORK_PROXY_H_

#include <gmock/gmock.h>

#include "shill/modem_gsm_network_proxy_interface.h"

namespace shill {

class MockModemGSMNetworkProxy : public ModemGSMNetworkProxyInterface {
 public:
  MOCK_METHOD1(Register, void(const std::string &network_id));
};

}  // namespace shill

#endif  // SHILL_MOCK_MODEM_GSM_NETWORK_PROXY_H_
