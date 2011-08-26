// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_DBUS_PROPERTIES_PROXY_H_
#define SHILL_MOCK_DBUS_PROPERTIES_PROXY_H_

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/dbus_properties_proxy_interface.h"

namespace shill {

class MockDBusPropertiesProxy : public DBusPropertiesProxyInterface {
 public:
  MockDBusPropertiesProxy();
  virtual ~MockDBusPropertiesProxy();

  MOCK_METHOD1(GetAll, DBusPropertiesMap(const std::string &interface_name));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDBusPropertiesProxy);
};

}  // namespace shill

#endif  // SHILL_MOCK_DBUS_PROPERTIES_PROXY_H_
