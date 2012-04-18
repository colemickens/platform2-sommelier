// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MODEM_MANAGER_H_
#define SHILL_MODEM_MANAGER_H_

#include <map>
#include <string>
#include <tr1/memory>
#include <vector>

#include <base/basictypes.h>
#include <base/memory/scoped_ptr.h>
#include <base/memory/weak_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/dbus_objectmanager_proxy_interface.h"
#include "shill/dbus_properties_proxy_interface.h"
#include "shill/glib.h"

struct mobile_provider_db;

namespace shill {

class ControlInterface;

class DBusObjectManagerProxyInterface;
class DBusPropertiesProxyInterface;
class EventDispatcher;
class Manager;
class Metrics;
class Modem1;
class Modem;
class ModemClassic;
class ModemManagerProxyInterface;
class ProxyFactory;

// Handles a modem manager service and creates and destroys modem instances.
class ModemManager {
 public:
  ModemManager(const std::string &service,
               const std::string &path,
               ControlInterface *control_interface,
               EventDispatcher *dispatcher,
               Metrics *metrics,
               Manager *manager,
               GLib *glib,
               mobile_provider_db *provider_db);
  virtual ~ModemManager();

  // Starts watching for and handling the DBus modem manager service.
  void Start();

  // Stops watching for the DBus modem manager service and destroys any
  // associated modems.
  void Stop();

  void OnDeviceInfoAvailable(const std::string &link_name);

 protected:
  typedef std::map<std::string, std::tr1::shared_ptr<Modem> > Modems;

  ControlInterface *control_interface() const { return control_interface_; }
  EventDispatcher *dispatcher() const { return dispatcher_; }
  Manager *manager() const { return manager_; }
  Metrics *metrics() const { return metrics_; }
  const std::string &owner() const { return owner_; }
  const std::string &path() const { return path_; }
  ProxyFactory *proxy_factory() const { return proxy_factory_; }
  mobile_provider_db *provider_db() const { return provider_db_; }

  // Connect/Disconnect to a modem manager service.
  // Inheriting classes must call this superclass method.
  virtual void Connect(const std::string &owner);
  // Inheriting classes must call this superclass method.
  virtual void Disconnect();

  bool ModemExists(const std::string &path) const;
  // Put the modem into our modem map
  void RecordAddedModem(std::tr1::shared_ptr<Modem> modem);

  // Removes a modem on |path|.
  void RemoveModem(const std::string &path);

 private:
  friend class ModemManagerCoreTest;
  friend class ModemManagerClassicTest;
  friend class ModemManager1Test;

  FRIEND_TEST(ModemInfoTest, RegisterModemManager);
  FRIEND_TEST(ModemManager1Test, AddRemoveInterfaces);
  FRIEND_TEST(ModemManager1Test, Connect);
  FRIEND_TEST(ModemManagerClassicTest, Connect);
  FRIEND_TEST(ModemManagerCoreTest, AddRemoveModem);
  FRIEND_TEST(ModemManagerCoreTest, Connect);
  FRIEND_TEST(ModemManagerCoreTest, Disconnect);
  FRIEND_TEST(ModemManagerCoreTest, ModemExists);
  FRIEND_TEST(ModemManagerCoreTest, OnAppearVanish);
  FRIEND_TEST(ModemManagerCoreTest, Start);
  FRIEND_TEST(ModemManagerCoreTest, Stop);

  // DBus service watcher callbacks.
  static void OnAppear(GDBusConnection *connection,
                       const gchar *name,
                       const gchar *name_owner,
                       gpointer user_data);
  static void OnVanish(GDBusConnection *connection,
                       const gchar *name,
                       gpointer user_data);

  // Store cached copies of singletons for speed/ease of testing.
  ProxyFactory *proxy_factory_;

  const std::string service_;
  const std::string path_;
  guint watcher_id_;

  std::string owner_;  // DBus service owner.

  Modems modems_;  // Maps a modem |path| to a modem instance.

  ControlInterface *control_interface_;
  EventDispatcher *dispatcher_;
  Metrics *metrics_;
  Manager *manager_;
  GLib *glib_;
  mobile_provider_db *provider_db_;

  DISALLOW_COPY_AND_ASSIGN(ModemManager);
};

class ModemManagerClassic : public ModemManager {
 public:
  ModemManagerClassic(const std::string &service,
                      const std::string &path,
                      ControlInterface *control_interface,
                      EventDispatcher *dispatcher,
                      Metrics *metrics,
                      Manager *manager,
                      GLib *glib,
                      mobile_provider_db *provider_db);

  virtual ~ModemManagerClassic();

  // Called by our dbus proxy
  void OnDeviceAdded(const std::string &path);
  void OnDeviceRemoved(const std::string &path);

 protected:
  virtual void Connect(const std::string &owner);
  virtual void Disconnect();

  virtual void AddModemClassic(const std::string &path);
  virtual void InitModemClassic(std::tr1::shared_ptr<ModemClassic> modem);

 private:
  scoped_ptr<ModemManagerProxyInterface> proxy_;  // DBus service proxy
  scoped_ptr<DBusPropertiesProxyInterface> dbus_properties_proxy_;

  FRIEND_TEST(ModemManagerClassicTest, Connect);

  DISALLOW_COPY_AND_ASSIGN(ModemManagerClassic);
};

class ModemManager1 : public ModemManager {
 public:
  ModemManager1(const std::string &service,
                const std::string &path,
                ControlInterface *control_interface,
                EventDispatcher *dispatcher,
                Metrics *metrics,
                Manager *manager,
                GLib *glib,
                mobile_provider_db *provider_db);

  virtual ~ModemManager1();

 protected:
  void AddModem1(const std::string &path,
                 const DBusInterfaceToProperties &i_to_p);
  virtual void InitModem1(std::tr1::shared_ptr<Modem1> modem,
                          const DBusInterfaceToProperties &i_to_p);

  // ModemManager methods
  virtual void Connect(const std::string &owner);
  virtual void Disconnect();

  // DBusObjectManagerProxyDelegate signal methods
  virtual void OnInterfacesAddedSignal(
      const ::DBus::Path &object_path,
      const DBusInterfaceToProperties &interface_to_properties);
  virtual void OnInterfacesRemovedSignal(
      const ::DBus::Path &object_path,
      const std::vector<std::string> &interfaces);

  // DBusObjectManagerProxyDelegate method callbacks
  virtual void OnGetManagedObjectsReply(
      const DBusObjectsWithProperties &objects_with_properties,
      const Error &error);

 private:
  FRIEND_TEST(ModemManager1Test, Connect);
  FRIEND_TEST(ModemManager1Test, AddRemoveInterfaces);

  scoped_ptr<DBusObjectManagerProxyInterface> proxy_;
  base::WeakPtrFactory<ModemManager1> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ModemManager1);
};
}  // namespace shill

#endif  // SHILL_MODEM_MANAGER_H_
