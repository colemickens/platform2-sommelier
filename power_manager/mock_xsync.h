// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_MOCK_XSYNC_H_
#define POWER_MANAGER_MOCK_XSYNC_H_

#include <set>

#include "base/time.h"
#include "power_manager/signal_callback.h"
#include "power_manager/xsync.h"

namespace power_manager {

class MockXSync : public XSync {
 public:
  MockXSync();

  // Virtual functions overridden from XSync
  virtual void Init() {}

  virtual bool QueryExtension(int* event_base, int* error_base);
  virtual bool Initialize(int* /* major_version */, int* /* minor_version */) {
    return true;
  }

  virtual void SetEventHandler(GdkFilterFunc func, gpointer data);

  virtual XSyncSystemCounter* ListSystemCounters(int* ncounters);
  virtual void FreeSystemCounterList(XSyncSystemCounter* counters);
  virtual bool QueryCounterInt64(XSyncCounter counter, int64* value);
  virtual bool QueryCounter(XSyncCounter counter, XSyncValue* value);

  virtual XSyncAlarm CreateAlarm(uint64 mask, XSyncAlarmAttributes* attrs);
  virtual bool DestroyAlarm(XSyncAlarm alarm);

  void FakeRelativeMotionEvent(int x, int y, uint64 delay);

 private:
  // Alarm object.
  struct MockAlarm {
    XSyncCounter counter;      // Idle counter ID.
    int64 idle_time;           // The idle time threshold for this alarm.
    bool positive_transition;  // Whether a positive or negative transition.
    int ref_count;             // When this reaches 0, delete the object.
  };

  // Alarm handler that simulates a gdk event and invokes the external event
  // handler function.
  SIGNAL_CALLBACK_PACKED_1(MockXSync, gboolean, TimeoutHandler, MockAlarm*);
  bool DestroyMockAlarm(MockAlarm* alarm);

  base::Time last_activity_time_;  // The last time there was user input.
  // External callback function and data that is invoked when there is an alarm
  // event.
  GdkFilterFunc event_handler_func;
  gpointer event_handler_data_;

  typedef std::set<MockAlarm*> AlarmSet;
  AlarmSet alarms_;  // Local set of alarms that were created by CreateAlarm.

  DISALLOW_COPY_AND_ASSIGN(MockXSync);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_MOCK_XSYNC_H_
