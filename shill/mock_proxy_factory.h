// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_PROXY_FACTORY_
#define SHILL_MOCK_PROXY_FACTORY_

#include <gmock/gmock.h>

#include "proxy_factory.h"

namespace shill {

class MockProxyFactory : public ProxyFactory {
 public:
  MockProxyFactory();
  virtual ~MockProxyFactory();

  MOCK_METHOD0(Init, void());
};

} // namespace shill

#endif  // SHILL_MOCK_PROXY_FACTORY_
