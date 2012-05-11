// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/xsync.h"

#include "base/logging.h"

namespace power_manager {

XSync::XSync()
    : display_(GDK_DISPLAY()) {
}

void XSync::Init() {
  if (!display_) {
    gdk_init_check(NULL, NULL);
    display_ = GDK_DISPLAY();
  }
  CHECK(display_) << "Display not initialized.";
}

bool XSync::QueryExtension(int* event_base, int* error_base) {
  return XSyncQueryExtension(display_, event_base, error_base);
}

bool XSync::Initialize(int* major_version, int* minor_version) {
  return XSyncInitialize(display_, major_version, minor_version);
}

XSyncSystemCounter* XSync::ListSystemCounters(int* ncounters) {
  return XSyncListSystemCounters(display_, ncounters);
}

void XSync::FreeSystemCounterList(XSyncSystemCounter* ncounters) {
  XSyncFreeSystemCounterList(ncounters);
}

bool XSync::QueryCounterInt64(XSyncCounter counter, int64* value) {
  XSyncValue xvalue;
  int retval = XSyncQueryCounter(display_, counter, &xvalue);
  *value = ValueToInt64(xvalue);
  return retval;
}

bool XSync::QueryCounter(XSyncCounter counter, XSyncValue* value) {
  return XSyncQueryCounter(display_, counter, value);
}

XSyncAlarm XSync::CreateAlarm(uint64 mask, XSyncAlarmAttributes* attrs) {
  return XSyncCreateAlarm(display_, static_cast<unsigned long>(mask), attrs);
}

bool XSync::DestroyAlarm(XSyncAlarm alarm) {
  return XSyncDestroyAlarm(display_, alarm);
}

void XSync::SetEventHandler(GdkFilterFunc func, gpointer data) {
  gdk_window_add_filter(NULL, func, data);
}

}  // namespace power_manager
