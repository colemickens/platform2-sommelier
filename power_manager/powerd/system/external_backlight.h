// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_EXTERNAL_BACKLIGHT_H_
#define POWER_MANAGER_POWERD_SYSTEM_EXTERNAL_BACKLIGHT_H_

#include <glib.h>
#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/observer_list.h"
#include "power_manager/common/signal_callback.h"
#include "power_manager/powerd/system/backlight_interface.h"

typedef int gboolean;
struct udev;
struct udev_monitor;

namespace power_manager {
namespace system {

class ExternalBacklight : public BacklightInterface {
 public:
  ExternalBacklight();
  virtual ~ExternalBacklight();

  // Initialize the backlight object.
  // On success, return true; otherwise return false.
  bool Init();

  // Overridden from BacklightInterface:
  virtual void AddObserver(BacklightInterfaceObserver* observer) OVERRIDE;
  virtual void RemoveObserver(BacklightInterfaceObserver* observer) OVERRIDE;
  virtual bool GetMaxBrightnessLevel(int64* max_level) OVERRIDE;
  virtual bool GetCurrentBrightnessLevel(int64* current_level) OVERRIDE;
  virtual bool SetBrightnessLevel(int64 level, base::TimeDelta interval)
      OVERRIDE;
  virtual bool SetResumeBrightnessLevel(int64 level) OVERRIDE;

 private:
  // This contains a set of i2c devices, represented as name-handle pairs.
  typedef std::map<std::string, int> I2CDeviceList;

  // Handles i2c and display  udev events.
  static gboolean UdevEventHandler(GIOChannel* source,
                                   GIOCondition condition,
                                   gpointer data);

  // Registers udev event handler with GIO.
  void RegisterUdevEventHandler();

  // Looks for available display devices.
  SIGNAL_CALLBACK_0(ExternalBacklight, gboolean, ScanForDisplays);

  // Indicates that there is a valid display device handle.
  bool HasValidHandle() const { return !primary_device_.empty(); }

  // Reads the current and maximum brightness levels into the supplied pointers.
  // Either pointer can be NULL.
  bool ReadBrightnessLevels(int64* current_level, int64* max_level);

  ObserverList<BacklightInterfaceObserver> observers_;

  // A list of display devices currently connected via i2c.
  I2CDeviceList display_devices_;

  // The primary display device.
  std::string primary_device_;

  // For listening to udev events.
  struct udev_monitor* udev_monitor_;
  struct udev* udev_;

  // GLib source ID for ScanForDisplays() timeout, or 0 if unscheduled.
  guint scan_for_displays_timeout_id_;

  DISALLOW_COPY_AND_ASSIGN(ExternalBacklight);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_EXTERNAL_BACKLIGHT_H_
