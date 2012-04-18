// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DBUS_PROPERTIES_PROXY_INTERFACE_
#define SHILL_DBUS_PROPERTIES_PROXY_INTERFACE_

#include <vector>

#include <base/callback.h>

#include "shill/dbus_properties.h"

namespace shill {

// These are the methods that a DBusProperties proxy must support. The interface
// is provided so that it can be mocked in tests.
class DBusPropertiesProxyInterface {
 public:
  // Callback invoked when an object sends a DBus property change signal.
  typedef base::Callback<void(
      const std::string &interface,
      const DBusPropertiesMap &changed_properties,
      const std::vector<std::string> &invalidated_properties)>
    PropertiesChangedCallback;

  // Callback invoked when the classic modem manager sends a DBus
  // property change signal.
  typedef base::Callback<void(
      const std::string &interface,
      const DBusPropertiesMap &properties)>
    ModemManagerPropertiesChangedCallback;

  virtual ~DBusPropertiesProxyInterface() {}

  virtual DBusPropertiesMap GetAll(const std::string &interface_name) = 0;

  virtual void set_properties_changed_callback(
      const PropertiesChangedCallback &callback) = 0;
  virtual void set_modem_manager_properties_changed_callback(
      const ModemManagerPropertiesChangedCallback &callback) = 0;
};

}  // namespace shill

#endif  // SHILL_DBUS_PROPERTIES_PROXY_INTERFACE_
