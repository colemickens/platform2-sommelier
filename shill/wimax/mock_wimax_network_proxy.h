// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_WIMAX_MOCK_WIMAX_NETWORK_PROXY_H_
#define SHILL_WIMAX_MOCK_WIMAX_NETWORK_PROXY_H_

#include <string>

#include <gmock/gmock.h>

#include "shill/wimax/wimax_network_proxy_interface.h"

namespace shill {

class MockWiMaxNetworkProxy : public WiMaxNetworkProxyInterface {
 public:
  MockWiMaxNetworkProxy();
  ~MockWiMaxNetworkProxy() override;

  MOCK_CONST_METHOD0(path, RpcIdentifier());
  MOCK_METHOD1(set_signal_strength_changed_callback,
               void(const SignalStrengthChangedCallback& callback));
  MOCK_METHOD1(Identifier, uint32_t(Error* error));
  MOCK_METHOD1(Name, std::string(Error* error));
  MOCK_METHOD1(Type, int(Error* error));
  MOCK_METHOD1(CINR, int(Error* error));
  MOCK_METHOD1(RSSI, int(Error* error));
  MOCK_METHOD1(SignalStrength, int(Error* error));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWiMaxNetworkProxy);
};

}  // namespace shill

#endif  // SHILL_WIMAX_MOCK_WIMAX_NETWORK_PROXY_H_
