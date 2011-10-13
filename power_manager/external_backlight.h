// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_EXTERNAL_BACKLIGHT_H_
#define POWER_MANAGER_EXTERNAL_BACKLIGHT_H_

#include <gdk/gdkx.h>
#include <libudev.h>
#include <list>
#include <map>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "power_manager/backlight_interface.h"
#include "power_manager/signal_callback.h"

typedef int gboolean;  // Forward declaration of bool type used by glib.

namespace power_manager {

class ExternalBacklight : public BacklightInterface {
 public:
  ExternalBacklight();
  virtual ~ExternalBacklight();

  // Initialize the backlight object.
  // On success, return true; otherwise return false.
  bool Init();

  // Overridden from BacklightInterface:
  virtual bool GetBrightness(int64* level, int64* max);
  virtual bool SetBrightness(int64 level);

 private:
  // Handles i2c and display  udev events.
  static gboolean UdevEventHandler(GIOChannel* source,
                                   GIOCondition condition,
                                   gpointer data);

  // Registers udev event handler with GIO.
  void RegisterUdevEventHandler();

  // Looks for available display devices.
  SIGNAL_CALLBACK_0(ExternalBacklight, gboolean, ScanForDisplays);

  // Indicates to other processes that the display device has changed.
  void SendDisplayChangedSignal();

  // Indicates that there is a valid display device handle.
  bool HasValidHandle() { return (i2c_handle_ >= 0); }

  std::string i2c_path_;
  int i2c_handle_;

  // For listening to udev events.
  struct udev_monitor* udev_monitor_;
  struct udev* udev_;

  bool is_scan_scheduled_;  // Flag to prevent redundant device scans.

  DISALLOW_COPY_AND_ASSIGN(ExternalBacklight);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_BACKLIGHT_H_
