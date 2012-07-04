// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_EXTERNAL_BACKLIGHT_H_
#define POWER_MANAGER_EXTERNAL_BACKLIGHT_H_

#include <glib.h>
#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "power_manager/backlight_interface.h"
#include "power_manager/signal_callback.h"

typedef int gboolean;  // Forward declaration of bool type used by glib.

// Forward declarations of structs from libudev.h.
struct udev;
struct udev_monitor;

namespace power_manager {

class ExternalBacklight : public BacklightInterface {
 public:
  ExternalBacklight();
  virtual ~ExternalBacklight();

  // Initialize the backlight object.
  // On success, return true; otherwise return false.
  bool Init();

  // Overridden from BacklightInterface:
  virtual bool GetMaxBrightnessLevel(int64* max_level);
  virtual bool GetCurrentBrightnessLevel(int64* current_level);
  virtual bool SetBrightnessLevel(int64 level);

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

  // Indicates to other processes that the display device has changed.
  // Returns true if the signal was sent successfully.
  bool SendDisplayChangedSignal();

  // GLib wrapper timeout for retrying SendDisplayChangedSignal().
  SIGNAL_CALLBACK_0(ExternalBacklight, gboolean, RetrySendDisplayChangedSignal);

  // Indicates that there is a valid display device handle.
  bool HasValidHandle() const { return !primary_device_.empty(); }

  // Reads the current and maximum brightness levels into the supplied pointers.
  // Either pointer can be NULL.
  bool ReadBrightnessLevels(int64* current_level, int64* max_level);

  // A list of display devices currently connected via i2c.
  I2CDeviceList display_devices_;

  // The primary display device.
  std::string primary_device_;

  // For listening to udev events.
  struct udev_monitor* udev_monitor_;
  struct udev* udev_;

  bool is_scan_scheduled_;  // Flag to prevent redundant device scans.

  // Timeout ID for retrying a brightness read.
  guint retry_send_display_changed_source_id_;

  DISALLOW_COPY_AND_ASSIGN(ExternalBacklight);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_BACKLIGHT_H_
