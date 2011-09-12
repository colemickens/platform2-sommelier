// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_XSYNC_H_
#define POWER_MANAGER_XSYNC_H_

#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/extensions/sync.h>

#include "base/basictypes.h"

namespace power_manager {

class XSync {
 public:
  XSync();
  virtual ~XSync() {}
  virtual void Init();

  virtual bool QueryExtension(int* event_base, int* error_base);
  virtual bool Initialize(int* major_version, int* minor_version);

  // For XSync system counter access.
  virtual XSyncSystemCounter* ListSystemCounters(int* ncounters);
  virtual void FreeSystemCounterList(XSyncSystemCounter* ncounters);
  virtual bool QueryCounterInt64(XSyncCounter counter, int64* value);
  virtual bool QueryCounter(XSyncCounter counter, XSyncValue* value);

  // Create and delete XSync alarms.
  virtual XSyncAlarm CreateAlarm(uint64 mask, XSyncAlarmAttributes* attrs);
  virtual bool DestroyAlarm(XSyncAlarm alarm);
  // Provides an event handler for alarms.
  virtual void SetEventHandler(GdkFilterFunc func, gpointer data);

  // Convert between int64 and XSync values.
  static int64 ValueToInt64(XSyncValue value);
  static void Int64ToValue(XSyncValue* xvalue, int64 value);

 private:
  Display* display_;

  DISALLOW_COPY_AND_ASSIGN(XSync);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_XSYNC_H_
