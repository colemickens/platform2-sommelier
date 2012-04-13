// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/mock_xsync.h"

#include "base/logging.h"
#include "power_manager/xevent_observer.h"

namespace {

const XSyncCounter kIdleCounter = 0xdeadbeef;  // ID for mock idle time counter.
const XSyncCounter kOtherCounter = 0x1337cafe;  // ID for other counters.
const int kNumCounters = 4;  // Number of mock X system counters.
const int kIdleCounterIndex = 2;  // Index of idle counter in mock counter list.
const char kIdleCounterName[] = "IDLETIME";  // Name of idle mock counter.
const char kOtherCounterName[] = "OTHER";  // Name of all other mock counters.
const int kEventBase = 0;
const int kErrorBase = 0;

}  // namespace

namespace power_manager {

MockXSync::MockXSync()
    : last_activity_time_(0),
      current_time_(0),
      observer_(NULL) {}

bool MockXSync::QueryExtension(int* event_base, int* error_base) {
  if (event_base)
    *event_base = kEventBase;
  if (error_base)
    *error_base = kErrorBase;
  return true;
}

XSyncSystemCounter* MockXSync::ListSystemCounters(int* ncounters) {
  if (ncounters)
    *ncounters = kNumCounters;
  // Create a list of mock counters.
  XSyncSystemCounter* counters = new XSyncSystemCounter[kNumCounters];
  CHECK(counters);
  for (int i = 0; i < kNumCounters; ++i) {
    // Populate the counter list with indices and names.  Only one should be an
    // idle counter.
    counters[i].counter = (i == kIdleCounterIndex) ? kIdleCounter
                                                   : kOtherCounter;
    const char* name = (i == kIdleCounterIndex) ? kIdleCounterName
                                                : kOtherCounterName;
    counters[i].name = new char[strlen(name) + 1];
    CHECK(counters[i].name);
    // strcpy is safe here because we just allocated counters[i].name
    // to strlen(name)+1
    strcpy(counters[i].name, name);  // NOLINT(runtime/printf)
  }
  return counters;
}

void MockXSync::FreeSystemCounterList(XSyncSystemCounter* counters) {
  CHECK(counters);
  for (int i = 0; i < kNumCounters; i++)
    delete[] counters[i].name;
  delete[] counters;
}

bool MockXSync::QueryCounterInt64(XSyncCounter counter, int64* value) {
  if (counter != kIdleCounter)
    return false;
  CHECK(value);
  // Compute and return the time since last activity.
  *value = GetIdleTime();
  return true;
}

bool MockXSync::QueryCounter(XSyncCounter counter, XSyncValue* value) {
  int64 int_value;
  if (!QueryCounterInt64(counter, &int_value))
    return false;
  CHECK(value);
  XSyncInterface::Int64ToValue(value, int_value);
  return true;
}

XSyncAlarm MockXSync::CreateAlarm(uint64 mask, XSyncAlarmAttributes* attrs) {
  // The mock alarm system is designed to support the usage in XIdle.  It is not
  // guaranteed to support any other usage.
  CHECK(mask & XSyncCACounter);
  CHECK(mask & XSyncCAValue);
  CHECK(mask & XSyncCATestType);
  CHECK(mask & XSyncCADelta);
  CHECK(attrs);

  XSyncTestType test_type = attrs->trigger.test_type;
  CHECK(test_type == XSyncPositiveTransition ||
        test_type == XSyncNegativeTransition);
  XSyncCounter counter = attrs->trigger.counter;
  int64 wait_value = XSyncInterface::ValueToInt64(attrs->trigger.wait_value);
  // The idle value for a negative transition must be positive, otherwise it is
  // impossible to attain.  Idle time cannot become negative.
  if (test_type == XSyncNegativeTransition)
    CHECK_GT(wait_value, 0);
  // Not sure what delta means, but Xidle uses only 0, so require delta = 0.
  CHECK(XSyncValueIsZero(attrs->delta));

  // Create alarm object.
  MockAlarm* alarm = new MockAlarm;
  CHECK(alarm);
  alarm->counter = counter;
  alarm->idle_time = wait_value;
  alarm->ref_count = 1;

  int64 current_idle_time;
  CHECK(QueryCounterInt64(counter, &current_idle_time));

  alarm->positive_transition = (test_type == XSyncPositiveTransition);

  // Save this alarm locally until it is deleted.
  alarms_.insert(alarm);

  return reinterpret_cast<XSyncAlarm>(alarm);
}

bool MockXSync::DestroyAlarm(XSyncAlarm alarm) {
  return DestroyMockAlarm(reinterpret_cast<MockAlarm*>(alarm));
}

void MockXSync::AddObserver(XEventObserverInterface* observer) {
  CHECK_EQ(observer_, static_cast<void*>(NULL)) << "Already added observer.";
  observer_ = observer;
}

void MockXSync::RemoveObserver(XEventObserverInterface* observer) {
  CHECK_EQ(observer_, observer) << "Observer has not been added.";
  observer_ = NULL;
}

void MockXSync::FakeRelativeMotionEvent(int, int, uint64) {
  // Store the current idle time before coming out of idle.
  int64 idle_time = GetIdleTime();
  // Simulate an activity at the current time by resetting the time of last
  // activity to the current time.
  last_activity_time_ = GetTimeNow();

  // Since this brings the system out of idle, find and handle all negative
  // transition alarms.
  TestAlarms(false, idle_time);
}

void MockXSync::Run(int64 total_time, int64 interval) {
  // Simulate the passage of time by running a loop over the given interval.
  CHECK_GE(total_time, 0);
  CHECK_GT(interval, 0);
  for (int64 runtime = 0; runtime < total_time; runtime += interval) {
    TestAlarms(true, GetIdleTime());
    // Update the time counter.
    current_time_ += interval;
  }
}

void MockXSync::TimeoutHandler(MockAlarm *alarm) {
  CHECK(alarm);
  int64 current_idle_time;
  CHECK(QueryCounterInt64(alarm->counter, &current_idle_time));

  // Make sure the amount of idle time requested has indeed been reached, if
  // this is a positive transition alarm.
  if (alarm->positive_transition)
    CHECK(current_idle_time >= alarm->idle_time);
  XEventHandlerStatus handler_result = XEVENT_HANDLER_STOP;
  // Invoke the event handler callback if one has been provided.
  if (observer_) {
    // Create and initialize a new mock X event object.
    union {
      XSyncAlarmNotifyEvent alarm_event;
      XEvent event;
    } mock_event;
    memset(&mock_event, 0, sizeof(mock_event));

    XEvent* xevent = &mock_event.event;
    XSyncAlarmNotifyEvent* alarm_event = &mock_event.alarm_event;
    xevent->type = kEventBase + XSyncAlarmNotify;
    alarm_event->state = XSyncAlarmActive;

    XSyncInterface::Int64ToValue(&alarm_event->counter_value,
                                 current_idle_time);
    XSyncInterface::Int64ToValue(&alarm_event->alarm_value, alarm->idle_time);
    // Call the event handler to simulate what happens during an X event.
    handler_result = observer_->HandleXEvent(&mock_event.event);
  } else {
    LOG(WARNING) << "No event handler callback specified.";
  }

  // Release the alarm if necessary.
  if (handler_result == XEVENT_HANDLER_STOP)
    DestroyMockAlarm(alarm);
}

bool MockXSync::DestroyMockAlarm(MockAlarm* alarm) {
  CHECK(alarm);
  // This might be used in multiple places, so don't delete it unless the
  // current caller is the very last user.
  if (alarm->ref_count == 0)
    return false;
  alarm->ref_count -= 1;
  if (alarm->ref_count == 0) {
    CHECK(alarms_.find(alarm) != alarms_.end());
    // Remove the alarm from the local alarm list and delete it.
    alarms_.erase(alarm);
    delete alarm;
  }
  return true;
}

void MockXSync::TestAlarms(bool positive_transition, int64 idle_time) {
  AlarmSet::const_iterator iter;
  for (iter = alarms_.begin(); iter != alarms_.end(); ++iter) {
    MockAlarm* alarm = *iter;
    CHECK(alarm);
    // Disregard all the alarms that were not requested to be tested, either
    // negative or positive transitions.
    if (positive_transition != alarm->positive_transition)
      continue;
    // Invoke the alarm if the idle time had at least as long as the alarm's
    // required idle time.
    if (idle_time >= alarm->idle_time)
      TimeoutHandler(alarm);
  }
}

}  // namespace power_manager
