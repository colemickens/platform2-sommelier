// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DBUS_PROPERTIES_PROXY_INTERFACE_H_
#define SHILL_DBUS_PROPERTIES_PROXY_INTERFACE_H_

#include <string>
#include <vector>

#include <base/callback.h>

#include "shill/key_value_store.h"

namespace shill {

// This is a cellular-specific DBus Properties interface, as it supports
// cellular-specific signal (ModemManagerPropertiesChanged).
// These are the methods that a DBusProperties proxy must support. The interface
// is provided so that it can be mocked in tests.
class DBusPropertiesProxyInterface {
 public:
  // Callback invoked when an object sends a DBus property change signal.
  using PropertiesChangedCallback = base::Callback<void(
      const std::string& interface,
      const KeyValueStore& changed_properties,
      const std::vector<std::string>& invalidated_properties)>;

  // Callback invoked when the classic modem manager sends a DBus
  // property change signal.
  using ModemManagerPropertiesChangedCallback = base::Callback<void(
      const std::string& interface, const KeyValueStore& properties)>;

  virtual ~DBusPropertiesProxyInterface() = default;

  virtual KeyValueStore GetAll(const std::string& interface_name) = 0;
  virtual brillo::Any Get(const std::string& interface_name,
                          const std::string& property) = 0;

  virtual void set_properties_changed_callback(
      const PropertiesChangedCallback& callback) = 0;
  virtual void set_modem_manager_properties_changed_callback(
      const ModemManagerPropertiesChangedCallback& callback) = 0;
};

}  // namespace shill

#endif  // SHILL_DBUS_PROPERTIES_PROXY_INTERFACE_H_
