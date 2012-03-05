// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_DBUS_OBJECTMANAGER_PROXY_H_
#define SHILL_MOCK_DBUS_OBJECTMANAGER_PROXY_H_

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/dbus_objectmanager_proxy_interface.h"

namespace shill {

class MockDBusObjectManagerProxy : public DBusObjectManagerProxyInterface {
 public:
  MockDBusObjectManagerProxy();
  virtual ~MockDBusObjectManagerProxy();

  MOCK_METHOD2(GetManagedObjects, void(AsyncCallHandler *call_handler,
                                       int timeout));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDBusObjectManagerProxy);
};

}  // namespace shill

#endif  // SHILL_MOCK_DBUS_OBJECTMANAGER_PROXY_H_
