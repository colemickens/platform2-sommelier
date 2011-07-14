// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MODEM_INFO_
#define SHILL_MODEM_INFO_

#include <string>

#include <base/memory/scoped_vector.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

namespace shill {

class ControlInterface;
class EventDispatcher;
class GLib;
class Manager;
class ModemManager;

// Manages modem managers.
class ModemInfo {
 public:
  ModemInfo(ControlInterface *control_interface,
            EventDispatcher *dispatcher,
            Manager *manager,
            GLib *glib);
  ~ModemInfo();

  void Start();
  void Stop();

 private:
  friend class ModemInfoTest;
  FRIEND_TEST(ModemInfoTest, RegisterModemManager);
  FRIEND_TEST(ModemInfoTest, StartStop);

  static const char kCromoService[];
  static const char kCromoPath[];

  // Registers a new ModemManager service handler and starts it.
  void RegisterModemManager(const std::string &service,
                            const std::string &path);

  ScopedVector<ModemManager> modem_managers_;

  ControlInterface *control_interface_;
  EventDispatcher *dispatcher_;
  Manager *manager_;
  GLib *glib_;

  DISALLOW_COPY_AND_ASSIGN(ModemInfo);
};

}  // namespace shill

#endif  // SHILL_MODEM_INFO_
