// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_EXTERNAL_BACKLIGHT_CLIENT_H_
#define POWER_MANAGER_POWERD_EXTERNAL_BACKLIGHT_CLIENT_H_

#include <dbus/dbus-glib-lowlevel.h>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "power_manager/common/backlight_interface.h"

typedef int gboolean;  // Forward declaration of bool type used by glib.

namespace power_manager {

class ExternalBacklightClient : public BacklightInterface {
 public:
  ExternalBacklightClient() {}
  virtual ~ExternalBacklightClient() {}

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

  int64 level_;
  int64 max_level_;

  DISALLOW_COPY_AND_ASSIGN(ExternalBacklightClient);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_EXTERNAL_BACKLIGHT_CLIENT_H_
