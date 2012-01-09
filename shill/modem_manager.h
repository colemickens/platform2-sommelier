// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MODEM_MANAGER_
#define SHILL_MODEM_MANAGER_

#include <map>
#include <string>
#include <tr1/memory>

#include <base/basictypes.h>
#include <base/memory/scoped_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/glib.h"

struct mobile_provider_db;

namespace shill {

class ControlInterface;
class EventDispatcher;
class Manager;
class Modem;
class ModemManagerProxyInterface;
class ProxyFactory;

// Handles a modem manager service and creates and destroys modem instances.
class ModemManager {
 public:
  ModemManager(const std::string &service,
               const std::string &path,
               ControlInterface *control_interface,
               EventDispatcher *dispatcher,
               Manager *manager,
               GLib *glib,
               mobile_provider_db *provider_db);
  ~ModemManager();

  // Starts watching for and handling the DBus modem manager service.
  void Start();

  // Stops watching for the DBus modem manager service and destroys any
  // associated modems.
  void Stop();

  // Adds a modem on |path|.
  void AddModem(const std::string &path);

  // Removes a modem on |path|.
  void RemoveModem(const std::string &path);

  void OnDeviceInfoAvailable(const std::string &link_name);

 private:
  friend class ModemManagerTest;
  FRIEND_TEST(ModemInfoTest, RegisterModemManager);
  FRIEND_TEST(ModemManagerTest, AddRemoveModem);
  FRIEND_TEST(ModemManagerTest, Connect);
  FRIEND_TEST(ModemManagerTest, Disconnect);
  FRIEND_TEST(ModemManagerTest, OnAppear);
  FRIEND_TEST(ModemManagerTest, OnVanish);
  FRIEND_TEST(ModemManagerTest, Start);
  FRIEND_TEST(ModemManagerTest, Stop);

  typedef std::map<std::string, std::tr1::shared_ptr<Modem> > Modems;

  // Connects a newly appeared modem manager service.
  void Connect(const std::string &owner);

  // Disconnects a vanished modem manager service.
  void Disconnect();

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
  scoped_ptr<ModemManagerProxyInterface> proxy_;  // DBus service proxy.

  Modems modems_;  // Maps a modem |path| to a modem instance.

  ControlInterface *control_interface_;
  EventDispatcher *dispatcher_;
  Manager *manager_;
  GLib *glib_;
  mobile_provider_db *provider_db_;

  DISALLOW_COPY_AND_ASSIGN(ModemManager);
};

}  // namespace shill

#endif  // SHILL_MODEM_MANAGER_
