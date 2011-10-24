// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DBUS_PROPERTIES_PROXY_INTERFACE_
#define SHILL_DBUS_PROPERTIES_PROXY_INTERFACE_

#include <vector>

#include "shill/dbus_properties.h"

namespace shill {

// These are the methods that a DBusProperties proxy must support. The interface
// is provided so that it can be mocked in tests.
class DBusPropertiesProxyInterface {
 public:
  virtual ~DBusPropertiesProxyInterface() {}

  virtual DBusPropertiesMap GetAll(const std::string &interface_name) = 0;
};

// DBus.Properties signal delegate to be associated with the proxy.
class DBusPropertiesProxyDelegate {
 public:
  virtual ~DBusPropertiesProxyDelegate() {}

  virtual void OnDBusPropertiesChanged(
      const std::string &interface,
      const DBusPropertiesMap &changed_properties,
      const std::vector<std::string> &invalidated_properties) = 0;

  virtual void OnModemManagerPropertiesChanged(
      const std::string &interface,
      const DBusPropertiesMap &properties) = 0;
};

}  // namespace shill

#endif  // SHILL_DBUS_PROPERTIES_PROXY_INTERFACE_
