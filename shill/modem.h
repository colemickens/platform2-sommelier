// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MODEM_
#define SHILL_MODEM_

#include <string>

#include <base/basictypes.h>
#include <base/memory/scoped_ptr.h>
#include <base/task.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/dbus_properties_proxy_interface.h"
#include "shill/refptr_types.h"

namespace shill {

class ControlInterface;
class EventDispatcher;
class Manager;

// Handles an instance of ModemManager.Modem and an instance of a Cellular
// device.
class Modem : public DBusPropertiesProxyListener {
 public:
  // |owner| is the ModemManager DBus service owner (e.g., ":1.17"). |path| is
  // the ModemManager.Modem DBus object path (e.g.,
  // "/org/chromium/ModemManager/Gobi/0").
  Modem(const std::string &owner,
        const std::string &path,
        ControlInterface *control_interface,
        EventDispatcher *dispatcher,
        Manager *manager);
  ~Modem();

  // Asynchronously initializes support for the modem, possibly constructing a
  // Cellular device.
  void Init();

 private:
  friend class ModemManagerTest;
  friend class ModemTest;
  FRIEND_TEST(ModemManagerTest, Connect);
  FRIEND_TEST(ModemManagerTest, AddRemoveModem);
  FRIEND_TEST(ModemTest, CreateCellularDevice);
  FRIEND_TEST(ModemTest, Init);

  static const char kPropertyAccessTechnology[];
  static const char kPropertyLinkName[];
  static const char kPropertyIPMethod[];
  static const char kPropertyState[];
  static const char kPropertyType[];
  static const char kPropertyUnlockRequired[];
  static const char kPropertyUnlockRetries[];

  void InitTask();

  // Creates and registers a Cellular device in |device_| based on
  // ModemManager.Modem's |properties|. The device may not be created if the
  // properties are invalid.
  void CreateCellularDevice(const DBusPropertiesMap &properties);

  // Signal callbacks inherited from DBusPropertiesProxyListener.
  virtual void OnDBusPropertiesChanged(
      const std::string &interface,
      const DBusPropertiesMap &changed_properties,
      const std::vector<std::string> &invalidated_properties);
  virtual void OnModemManagerPropertiesChanged(
      const std::string &interface,
      const DBusPropertiesMap &properties);

  // A proxy to the org.freedesktop.DBus.Properties interface used to obtain
  // ModemManager.Modem properties and watch for property changes.
  scoped_ptr<DBusPropertiesProxyInterface> dbus_properties_proxy_;

  const std::string owner_;
  const std::string path_;

  CellularRefPtr device_;

  ScopedRunnableMethodFactory<Modem> task_factory_;
  ControlInterface *control_interface_;
  EventDispatcher *dispatcher_;
  Manager *manager_;

  DISALLOW_COPY_AND_ASSIGN(Modem);
};

}  // namespace shill

#endif  // SHILL_MODEM_
