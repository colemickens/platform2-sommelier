// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MODEM_MANAGER_
#define SHILL_MODEM_MANAGER_

#include <base/basictypes.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/glib.h"

namespace shill {

class ControlInterface;
class EventDispatcher;
class Manager;

// Handles a modem manager service and creates and destroys cellular devices.
class ModemManager {
 public:
  ModemManager(const std::string &service,
               const std::string &path,
               ControlInterface *control_interface,
               EventDispatcher *dispatcher,
               Manager *manager,
               GLib *glib);
  ~ModemManager();

  // Starts watching for and handling the DBus modem manager service.
  void Start();

  // Stops watching for the DBus modem manager service and destroys any
  // associated modem devices.
  void Stop();

 private:
  friend class ModemManagerTest;
  FRIEND_TEST(ModemInfoTest, RegisterModemManager);
  FRIEND_TEST(ModemManagerTest, Connect);
  FRIEND_TEST(ModemManagerTest, Disconnect);
  FRIEND_TEST(ModemManagerTest, OnAppear);
  FRIEND_TEST(ModemManagerTest, OnVanish);
  FRIEND_TEST(ModemManagerTest, Start);
  FRIEND_TEST(ModemManagerTest, Stop);

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

  const std::string service_;
  const std::string path_;
  guint watcher_id_;

  // DBus service owner.
  std::string owner_;

  ControlInterface *control_interface_;
  EventDispatcher *dispatcher_;
  Manager *manager_;
  GLib *glib_;

  DISALLOW_COPY_AND_ASSIGN(ModemManager);
};

}  // namespace shill

#endif  // SHILL_MODEM_MANAGER_
