// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MODEM_H_
#define SHILL_MODEM_H_

#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/file_util.h>
#include <base/memory/scoped_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/cellular.h"
#include "shill/dbus_objectmanager_proxy_interface.h"
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
class Modem {
 public:
  // |owner| is the ModemManager DBus service owner (e.g., ":1.17"). |path| is
  // the ModemManager.Modem DBus object path (e.g.,
  // "/org/chromium/ModemManager/Gobi/0").
  Modem(const std::string &owner,
        const std::string &service,
        const std::string &path,
        ControlInterface *control_interface,
        EventDispatcher *dispatcher,
        Metrics *metrics,
        Manager *manager,
        mobile_provider_db *provider_db);
  ~Modem();

  // Asynchronously initializes support for the modem.
  // If the |properties| are valid and the MAC address is present,
  // constructs and registers a Cellular device in |device_| based on
  // |properties|.
  virtual void CreateDeviceFromModemProperties(
      const DBusInterfaceToProperties &properties);

  void OnDeviceInfoAvailable(const std::string &link_name);

  const std::string &owner() const { return owner_; }
  const std::string &service() const { return service_; }
  const std::string &path() const { return path_; }

  void set_type(Cellular::Type type) { type_ = type; }

 protected:
  static const char kPropertyLinkName[];
  static const char kPropertyIPMethod[];
  static const char kPropertyType[];

  virtual void Init();

  ControlInterface *control_interface() const { return control_interface_; }
  CellularRefPtr device() const { return device_; }
  EventDispatcher *dispatcher() const { return dispatcher_; }
  Manager *manager() const { return manager_; }
  Metrics *metrics() const { return metrics_; }
  mobile_provider_db *provider_db() const { return provider_db_; }

  virtual Cellular *ConstructCellular(const std::string &link_name,
                                      const std::string &device_name,
                                      int interface_index);
  virtual bool GetLinkName(const DBusPropertiesMap &properties,
                           std::string *name) const = 0;
  // Returns the name of the DBUS Modem interface.
  virtual std::string GetModemInterface(void) const = 0;

 private:
  friend class ModemTest;
  friend class Modem1Test;
  FRIEND_TEST(Modem1Test, Init);
  FRIEND_TEST(Modem1Test, CreateDeviceMM1);
  FRIEND_TEST(ModemManager1Test, Connect);
  FRIEND_TEST(ModemManager1Test, AddRemoveInterfaces);
  FRIEND_TEST(ModemManagerClassicTest, Connect);
  FRIEND_TEST(ModemManagerCoreTest, ShouldAddModem);
  FRIEND_TEST(ModemTest, CreateDeviceEarlyFailures);
  FRIEND_TEST(ModemTest, EarlyDeviceProperties);
  FRIEND_TEST(ModemTest, Init);
  FRIEND_TEST(ModemTest, PendingDevicePropertiesAndCreate);

  virtual void OnDBusPropertiesChanged(
      const std::string &interface,
      const DBusPropertiesMap &changed_properties,
      const std::vector<std::string> &invalidated_properties);
  virtual void OnModemManagerPropertiesChanged(
      const std::string &interface,
      const DBusPropertiesMap &properties);

  // A proxy to the org.freedesktop.DBusProperties interface used to obtain
  // ModemManager.Modem properties and watch for property changes
  scoped_ptr<DBusPropertiesProxyInterface> dbus_properties_proxy_;

  DBusInterfaceToProperties initial_properties_;

  const std::string owner_;
  const std::string service_;
  const std::string path_;

  CellularRefPtr device_;

  ControlInterface *control_interface_;
  EventDispatcher *dispatcher_;
  Metrics *metrics_;
  Manager *manager_;
  mobile_provider_db *provider_db_;
  std::string link_name_;
  Cellular::Type type_;
  bool pending_device_info_;
  RTNLHandler *rtnl_handler_;

  // Store cached copies of singletons for speed/ease of testing.
  ProxyFactory *proxy_factory_;

  DISALLOW_COPY_AND_ASSIGN(Modem);
};

class ModemClassic : public Modem {
 public:
  ModemClassic(const std::string &owner,
               const std::string &service,
               const std::string &path,
               ControlInterface *control_interface,
               EventDispatcher *dispatcher,
               Metrics *metrics,
               Manager *manager,
               mobile_provider_db *provider_db);
  virtual ~ModemClassic();

  // Gathers information and passes it to CreateDeviceFromModemProperties.
  void CreateDeviceClassic(const DBusPropertiesMap &modem_properties);

 protected:
  virtual bool GetLinkName(const DBusPropertiesMap &modem_properties,
                           std::string *name) const;
  virtual std::string GetModemInterface(void) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(ModemClassic);
};

class Modem1 : public Modem {
 public:
  Modem1(const std::string &owner,
         const std::string &service,
         const std::string &path,
         ControlInterface *control_interface,
         EventDispatcher *dispatcher,
         Metrics *metrics,
         Manager *manager,
         mobile_provider_db *provider_db);
  virtual ~Modem1();

  // Gathers information and passes it to CreateDeviceFromModemProperties.
  void CreateDeviceMM1(const DBusInterfaceToProperties &properties);

 protected:
  virtual bool GetLinkName(const DBusPropertiesMap &modem_properties,
                           std::string *name) const;
  virtual std::string GetModemInterface(void) const;

 private:
  friend class Modem1Test;

  FilePath netfiles_path_;  // Used for testing

  DISALLOW_COPY_AND_ASSIGN(Modem1);
};

}  // namespace shill

#endif  // SHILL_MODEM_H_
