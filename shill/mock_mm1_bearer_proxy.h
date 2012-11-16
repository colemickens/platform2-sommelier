// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MM1_MOCK_BEARER_PROXY_H_
#define SHILL_MM1_MOCK_BEARER_PROXY_H_

#include <string>

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/mm1_bearer_proxy_interface.h"

namespace shill {
namespace mm1 {

class MockBearerProxy : public BearerProxyInterface {
 public:
  MockBearerProxy();
  virtual ~MockBearerProxy();

  MOCK_METHOD3(Connect, void(Error *error,
                             const ResultCallback &callback,
                             int timeout));
  MOCK_METHOD3(Disconnect, void(Error *error,
                                const ResultCallback &callback,
                                int timeout));

  MOCK_METHOD0(Interface, const std::string());
  MOCK_METHOD0(Connected, bool());
  MOCK_METHOD0(Suspended, bool());
  MOCK_METHOD0(Ip4Config, const DBusPropertiesMap());
  MOCK_METHOD0(Ip6Config, const DBusPropertiesMap());
  MOCK_METHOD0(IpTimeout, uint32_t());
  MOCK_METHOD0(Properties, const DBusPropertiesMap());
};

}  // namespace mm1
}  // namespace shill

#endif  // SHILL_MM1_MOCK_BEARER_PROXY_H_
