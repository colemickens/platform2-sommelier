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

  // Simulates user input.
  void FakeRelativeMotionEvent(int x, int y, uint64 delay);

  // Simplified version of FakeRelativeMotionEvent().
  // The parameters aren't used anyway, so this wrapper is a convenient way for
  // unit tests to simulate user input without the clutter of params.
  void FakeMotionEvent() {
    FakeRelativeMotionEvent(0, 0, 0);
  }

  // Simulates time using a static loop.
  void Run(int64 total_time, int64 interval);

  // Resets the simulated time.
  void Reset() {
    current_time_ = 0;
  }

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

  // Returns the current time and time since last activity, respectively.  Note
  // that these are loop-simulated times, not actual system clock-based times.
  int64 GetTimeNow() {
    return current_time_;
  }
  int64 GetIdleTime() {
    return GetTimeNow() - last_activity_time_;
  }

  // Check if any of the positive or negative transition alarms have been
  // triggered by an idle time, and invoke the timeout handler when appropriate.
  void TestAlarms(bool positive_transition, int64 idle_time);

  int64 last_activity_time_;  // The last time there was user input.
  int64 current_time_;        // Mock current time, a loop-simulated value.
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
