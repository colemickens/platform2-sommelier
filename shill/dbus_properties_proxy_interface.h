// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DBUS_PROPERTIES_PROXY_INTERFACE_
#define SHILL_DBUS_PROPERTIES_PROXY_INTERFACE_

#include <map>
#include <string>

#include <dbus-c++/types.h>

namespace shill {

// These are the methods that a DBusProperties proxy must support. The interface
// is provided so that it can be mocked in tests.
class DBusPropertiesProxyInterface {
 public:
  virtual ~DBusPropertiesProxyInterface() {}

  virtual std::map<std::string, DBus::Variant> GetAll(
      const std::string &interface_name) = 0;
};

}  // namespace shill

#endif  // SHILL_DBUS_PROPERTIES_PROXY_INTERFACE_
