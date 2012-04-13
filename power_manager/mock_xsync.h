// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_MOCK_XSYNC_H_
#define POWER_MANAGER_MOCK_XSYNC_H_

#include <set>

#include "base/compiler_specific.h"
#include "base/time.h"
#include "power_manager/signal_callback.h"
#include "power_manager/xsync_interface.h"

namespace power_manager {

class MockXSync : public XSyncInterface {
 public:
  MockXSync();
  virtual ~MockXSync() {}

  // Virtual functions overridden from XSync
  virtual void Init() OVERRIDE {}

  virtual bool QueryExtension(int* event_base, int* error_base) OVERRIDE;
  virtual bool Initialize(int* /* major_version */,
                          int* /* minor_version */) OVERRIDE {
    return true;
  }

  virtual XSyncSystemCounter* ListSystemCounters(int* ncounters) OVERRIDE;
  virtual void FreeSystemCounterList(XSyncSystemCounter* counters) OVERRIDE;
  virtual bool QueryCounterInt64(XSyncCounter counter, int64* value) OVERRIDE;
  virtual bool QueryCounter(XSyncCounter counter, XSyncValue* value) OVERRIDE;

  virtual XSyncAlarm CreateAlarm(uint64 mask,
                                 XSyncAlarmAttributes* attrs) OVERRIDE;
  virtual bool DestroyAlarm(XSyncAlarm alarm) OVERRIDE;

  virtual void AddObserver(XEventObserverInterface* observer) OVERRIDE;
  virtual void RemoveObserver(XEventObserverInterface* observer) OVERRIDE;

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

  // Alarm handler that simulates an X event and invokes the external event
  // handler function.
  void TimeoutHandler(MockAlarm*);

  // Removes an alarm.
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

  typedef std::set<MockAlarm*> AlarmSet;
  AlarmSet alarms_;  // Local set of alarms that were created by CreateAlarm.

  // The mock XSync object keeps track of only one observer.
  XEventObserverInterface* observer_;

  DISALLOW_COPY_AND_ASSIGN(MockXSync);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_MOCK_XSYNC_H_
