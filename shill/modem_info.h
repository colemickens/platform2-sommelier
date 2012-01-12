// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MODEM_INFO_
#define SHILL_MODEM_INFO_

#include <string>

#include <base/memory/scoped_vector.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

struct mobile_provider_db;

namespace shill {

class ControlInterface;
class EventDispatcher;
class GLib;
class Manager;
class Metrics;
class ModemManager;

// Manages modem managers.
class ModemInfo {
 public:
  ModemInfo(ControlInterface *control_interface,
            EventDispatcher *dispatcher,
            Metrics *metrics,
            Manager *manager,
            GLib *glib);
  ~ModemInfo();

  void Start();
  void Stop();

  void OnDeviceInfoAvailable(const std::string &link_name);

 private:
  friend class ModemInfoTest;
  FRIEND_TEST(ModemInfoTest, RegisterModemManager);
  FRIEND_TEST(ModemInfoTest, StartStop);

  typedef ScopedVector<ModemManager> ModemManagers;

  static const char kCromoService[];
  static const char kCromoPath[];
  static const char kMobileProviderDBPath[];

  // Registers a new ModemManager service handler and starts it.
  void RegisterModemManager(const std::string &service,
                            const std::string &path);

  ModemManagers modem_managers_;

  ControlInterface *control_interface_;
  EventDispatcher *dispatcher_;
  Metrics *metrics_;
  Manager *manager_;
  GLib *glib_;

  std::string provider_db_path_;  // For testing.
  mobile_provider_db *provider_db_;  // Database instance owned by |this|.

  DISALLOW_COPY_AND_ASSIGN(ModemInfo);
};

}  // namespace shill

#endif  // SHILL_MODEM_INFO_
