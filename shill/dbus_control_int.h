// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DBUS_CONTROL_INT_H_
#define SHILL_DBUS_CONTROL_INT_

#include <string>

namespace shill {

// Superclass for all DBus-backed Proxy objects
class DBusProxy : public ProxyInterface {
 public:
  void SetProperty(const string &key, const string &value);
  const string *GetProperty(const string &key);
  void ClearProperty(const string &key);

 protected:
  string interface_;
  string path_;
};

class DBusControl;

// Subclass of DBusProxy for Manager objects
class ManagerDBusProxy : protected DBusProxy, public ManagerProxyInterface {
 public:
  explicit ManagerDBusProxy(Manager *manager);
  void UpdateRunning();

 private:
  static const char kInterfaceName[];
  static const char kPath[];
  string interface_;
  string path_;
  Manager *manager_;
};

// Subclass of DBusProxy for Service objects
class ServiceDBusProxy : protected DBusProxy, public ServiceProxyInterface {
 public:
  explicit ServiceDBusProxy(Service *service);
  void UpdateConnected();

 private:
  static const char kInterfaceName[];
  static const char kPath[];
  string interface_;
  string path_;
  Service *service_;
};

// Subclass of DBusProxy for Device objects
class DeviceDBusProxy : protected DBusProxy, public DeviceProxyInterface {
 public:
  explicit DeviceDBusProxy(Device *device);
  void UpdateEnabled();

 private:
  static const char kInterfaceName[];
  static const char kPath[];
  string interface_;
  string path_;
  Device *device_;
};

}  // namespace shill
#endif  // SHILL_DBUS_CONTROL_INT_
