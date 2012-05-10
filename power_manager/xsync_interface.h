// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_XSYNC_INTERFACE_H_
#define POWER_MANAGER_XSYNC_INTERFACE_H_

#include <X11/Xlib.h>
#include <X11/extensions/sync.h>

#include "base/basictypes.h"

namespace power_manager {

class XEventObserverInterface;

class XSyncInterface {
 public:
  virtual ~XSyncInterface() {}

  virtual void Init() = 0;

  // For X Sync extension initialization.
  virtual bool QueryExtension(int* event_base, int* error_base) = 0;
  virtual bool Initialize(int* major_version, int* minor_version) = 0;

  // For XSync system counter access.
  virtual XSyncSystemCounter* ListSystemCounters(int* num_counters) = 0;
  virtual void FreeSystemCounterList(XSyncSystemCounter* counters) = 0;
  virtual bool QueryCounterInt64(XSyncCounter counter, int64* value) = 0;
  virtual bool QueryCounter(XSyncCounter counter, XSyncValue* value) = 0;

  // Create and delete XSync alarms.
  virtual XSyncAlarm CreateAlarm(uint64 mask, XSyncAlarmAttributes* attrs) = 0;
  virtual bool DestroyAlarm(XSyncAlarm alarm) = 0;

  // Add and remove X event handlers.
  virtual void AddObserver(XEventObserverInterface* observer) = 0;
  virtual void RemoveObserver(XEventObserverInterface* observer) = 0;

  // Convert between int64 and XSync values.
  static int64 ValueToInt64(XSyncValue value) {
    // In the current X system, the idle time is returned as a 32-bit signed
    // value, but the sign bit is not extended to the higher 32-bit word part of
    // int XSyncValue.  It must instead be interpreted as an int32, to handle
    // the sign correctly.
    return static_cast<int32>(XSyncValueLow32(value));
  }
  static void Int64ToValue(XSyncValue* xvalue, int64 value) {
    XSyncIntsToValue(xvalue, value, value >> 32);
  }
};

}  // namespace power_manager

#endif  // POWER_MANAGER_XSYNC_INTERFACE_H_
