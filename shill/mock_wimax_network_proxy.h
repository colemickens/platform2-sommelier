// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_WIMAX_NETWORK_PROXY_H_
#define SHILL_MOCK_WIMAX_NETWORK_PROXY_H_

#include <gmock/gmock.h>

#include "shill/wimax_network_proxy_interface.h"

namespace shill {

class MockWiMaxNetworkProxy : public WiMaxNetworkProxyInterface {
 public:
  MockWiMaxNetworkProxy();
  virtual ~MockWiMaxNetworkProxy();

  MOCK_CONST_METHOD0(proxy_object_path, DBus::Path());
  MOCK_METHOD1(Identifier, uint32(Error *error));
  MOCK_METHOD1(Name, std::string(Error *error));
  MOCK_METHOD1(Type, int(Error *error));
  MOCK_METHOD1(CINR, int(Error *error));
  MOCK_METHOD1(RSSI, int(Error *error));
  MOCK_METHOD1(SignalStrength, int(Error *error));

  DISALLOW_COPY_AND_ASSIGN(MockWiMaxNetworkProxy);
};

}  // namespace shill

#endif  // SHILL_MOCK_WIMAX_NETWORK_PROXY_H_
