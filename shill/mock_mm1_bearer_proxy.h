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
};

}  // namespace mm1
}  // namespace shill

#endif  // SHILL_MM1_MOCK_BEARER_PROXY_H_
