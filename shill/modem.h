// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MODEM_
#define SHILL_MODEM_

#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/memory/scoped_ptr.h>
#include <base/memory/weak_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/dbus_properties_proxy_interface.h"
#include "shill/refptr_types.h"

struct mobile_provider_db;

namespace shill {

class ControlInterface;
class EventDispatcher;
class Manager;
class Metrics;
class ProxyFactory;

// Handles an instance of ModemManager.Modem and an instance of a Cellular
// device.
class Modem : public DBusPropertiesProxyDelegate {
 public:
  // |owner| is the ModemManager DBus service owner (e.g., ":1.17"). |path| is
  // the ModemManager.Modem DBus object path (e.g.,
  // "/org/chromium/ModemManager/Gobi/0").
  Modem(const std::string &owner,
        const std::string &path,
        ControlInterface *control_interface,
        EventDispatcher *dispatcher,
        Metrics *metrics,
        Manager *manager,
        mobile_provider_db *provider_db);
  ~Modem();

  // Asynchronously initializes support for the modem, possibly constructing a
  // Cellular device.
  void Init();

  void OnDeviceInfoAvailable(const std::string &link_name);

 private:
  friend class ModemTest;
  FRIEND_TEST(ModemManagerClassicTest, Connect);
  FRIEND_TEST(ModemManagerCoreTest, AddRemoveModem);
  FRIEND_TEST(ModemTest, CreateDeviceFromProperties);
  FRIEND_TEST(ModemTest, Init);

  static const char kPropertyLinkName[];
  static const char kPropertyIPMethod[];
  static const char kPropertyState[];
  static const char kPropertyType[];

  // Creates and registers a Cellular device in |device_| based on
  // ModemManager.Modem's |properties|. The device may not be created if the
  // properties are invalid.
  void CreateDeviceFromProperties(const DBusPropertiesMap &properties);
  void CreateDevice();

  // Signal callbacks inherited from DBusPropertiesProxyDelegate.
  virtual void OnDBusPropertiesChanged(
      const std::string &interface,
      const DBusPropertiesMap &changed_properties,
      const std::vector<std::string> &invalidated_properties);
  virtual void OnModemManagerPropertiesChanged(
      const std::string &interface,
      const DBusPropertiesMap &properties);

  // Store cached copies of singletons for speed/ease of testing.
  ProxyFactory *proxy_factory_;

  // A proxy to the org.freedesktop.DBus.Properties interface used to obtain
  // ModemManager.Modem properties and watch for property changes.
  scoped_ptr<DBusPropertiesProxyInterface> dbus_properties_proxy_;

  const std::string owner_;
  const std::string path_;

  CellularRefPtr device_;

  ControlInterface *control_interface_;
  EventDispatcher *dispatcher_;
  Metrics *metrics_;
  Manager *manager_;
  mobile_provider_db *provider_db_;
  std::string link_name_;
  bool pending_device_info_;

  DISALLOW_COPY_AND_ASSIGN(Modem);
};

}  // namespace shill

#endif  // SHILL_MODEM_
