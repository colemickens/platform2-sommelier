// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOCK_SUPPLICANT_NETWORK_PROXY_H_
#define MOCK_SUPPLICANT_NETWORK_PROXY_H_

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/supplicant_network_proxy_interface.h"

namespace shill {

class MockSupplicantNetworkProxy : public SupplicantNetworkProxyInterface {
 public:
  MockSupplicantNetworkProxy();
  virtual ~MockSupplicantNetworkProxy();

  MOCK_METHOD1(SetEnabled, void(bool enabled));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSupplicantNetworkProxy);
};

}  // namespace shill

#endif  // MOCK_SUPPLICANT_NETWORK_PROXY_H_
