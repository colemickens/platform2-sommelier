// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_BACKLIGHT_CLIENT_H_
#define POWER_MANAGER_POWERD_BACKLIGHT_CLIENT_H_

#include <dbus/dbus-glib-lowlevel.h>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "power_manager/common/backlight_interface.h"
#include "power_manager/common/power_constants.h"

typedef int gboolean;  // Forward declaration of bool type used by glib.

namespace power_manager {

// BacklightInterface implementation used by powerd that just forwards requests
// over D-Bus to powerm.
class BacklightClient : public BacklightInterface {
 public:
  explicit BacklightClient(BacklightType type);
  virtual ~BacklightClient();

  // Initialize the backlight object.
  // On success, return true; otherwise return false.
  bool Init();

  // Overridden from BacklightInterface:
  virtual bool GetMaxBrightnessLevel(int64* max_level);
  virtual bool GetCurrentBrightnessLevel(int64* current_level);
  virtual bool SetBrightnessLevel(int64 level);

 private:
  bool GetActualBrightness(int64* level, int64* max);

  void RegisterDBusMessageHandler();
  static DBusHandlerResult DBusMessageHandler(DBusConnection*,
                                              DBusMessage* message,
                                              void* data);

  BacklightType type_;

  int64 level_;
  int64 max_level_;

  DISALLOW_COPY_AND_ASSIGN(BacklightClient);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_BACKLIGHT_CLIENT_H_
