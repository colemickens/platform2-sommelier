// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWER_MANAGER_SERVICE_H_
#define POWER_MANAGER_POWER_MANAGER_SERVICE_H_

#include "base/basictypes.h"
#include "chromeos/dbus/abstract_dbus_service.h"
#include "chromeos/dbus/dbus.h"
#include "chromeos/dbus/service_constants.h"

namespace power_manager {

class Daemon;

namespace gobject {
struct PowerManager;
}  // namespace gobject

class PowerManagerService : public chromeos::dbus::AbstractDbusService {
 public:
  explicit PowerManagerService(Daemon* daemon);
  virtual ~PowerManagerService();

  // chromeos::dbus::AbstractDbusService implementation.
  virtual bool Initialize();
  virtual bool Register(const chromeos::dbus::BusConnection& conn);
  virtual bool Reset();

  virtual const char* service_name() const {
    return kPowerManagerServiceName;
  }
  virtual const char* service_path() const {
    return kPowerManagerServicePath;
  }
  virtual const char* service_interface() const {
    return kPowerManagerInterface;
  }
  virtual GObject* service_object() const {
    return G_OBJECT(power_manager_);
  }

  // PowerManagerService commands

  // Decreases the screen brightness, as often provided via a dedicated key
  // combination or button.
  //
  // If |allow_off| is FALSE, the backlight will never be entirely turned off.
  // This should be used with on-screen controls to prevent their becoming
  // impossible for the user to see.
  void DecreaseScreenBrightness(gboolean allow_off);

  // Increases the screen brightness, as often provided via a dedicated key
  // combination or button.
  void IncreaseScreenBrightness();

 protected:
  // chromeos::dbus::AbstractDbusService implementation.
  virtual GMainLoop* main_loop() { return NULL; }

 private:
  gobject::PowerManager* power_manager_;
  Daemon* daemon_;

  DISALLOW_COPY_AND_ASSIGN(PowerManagerService);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_POWER_MANAGER_SERVICE_H_
