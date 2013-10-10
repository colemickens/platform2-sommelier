// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_PROXY_FACTORY_
#define SHILL_MOCK_PROXY_FACTORY_

#include "shill/proxy_factory.h"

#include <gmock/gmock.h>

namespace shill {

class MockProxyFactory : public ProxyFactory {
 public:
  MockProxyFactory();
  virtual ~MockProxyFactory();

  MOCK_METHOD0(Init, void());
  MOCK_METHOD2(CreateSupplicantProcessProxy, SupplicantProcessProxyInterface *(
      const char *dbus_path, const char *dbus_addr));
  MOCK_METHOD3(CreateSupplicantInterfaceProxy,
               SupplicantInterfaceProxyInterface *(
                   SupplicantEventDelegateInterface *delegate,
                   const DBus::Path &object_path,
                   const char *dbus_addr));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockProxyFactory);
};

}  // namespace shill

#endif  // SHILL_MOCK_PROXY_FACTORY_
