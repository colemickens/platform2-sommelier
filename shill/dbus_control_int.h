// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DBUS_CONTROL_INT_H_
#define SHILL_DBUS_CONTROL_INT_

#include <string>

namespace shill {

// Superclass for all DBus-backed Adaptor objects
class DBusAdaptor : public AdaptorInterface {
 public:
  void SetProperty(const string &key, const string &value);
  const string *GetProperty(const string &key);
  void ClearProperty(const string &key);

 protected:
  string interface_;
  string path_;
};

class DBusControl;

// Subclass of DBusAdaptor for Manager objects
class ManagerDBusAdaptor : protected DBusAdaptor,
                           public ManagerAdaptorInterface {
 public:
  explicit ManagerDBusAdaptor(Manager *manager);
  void UpdateRunning();

 private:
  static const char kInterfaceName[];
  static const char kPath[];
  string interface_;
  string path_;
  Manager *manager_;
};

// Subclass of DBusAdaptor for Service objects
class ServiceDBusAdaptor : protected DBusAdaptor,
                           public ServiceAdaptorInterface {
 public:
  explicit ServiceDBusAdaptor(Service *service);
  void UpdateConnected();

 private:
  static const char kInterfaceName[];
  static const char kPath[];
  string interface_;
  string path_;
  Service *service_;
};

// Subclass of DBusAdaptor for Device objects
class DeviceDBusAdaptor : protected DBusAdaptor,
                          public DeviceAdaptorInterface {
 public:
  explicit DeviceDBusAdaptor(Device *device);
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
