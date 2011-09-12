// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/mock_xsync.h"

#include "base/logging.h"

namespace {

const XSyncCounter kIdleCounter = 0xdeadbeef;  // ID for mock idle time counter.
const XSyncCounter kOtherCounter = 0x1337cafe;  // ID for other counters.
const int kNumCounters = 4;  // Number of mock X system counters.
const int kIdleCounterIndex = 2;  // Index of idle counter in mock counter list.
const char kIdleCounterName[] = "IDLETIME";  // Name of idle mock counter.
const char kOtherCounterName[] = "OTHER";  // Name of all other mock counters.
const int kEventBase = 0;
const int kErrorBase = 0;

// Used for generating mock X events.
struct MockGdkXEvent {
  union {
    XSyncAlarmNotifyEvent alarm_event;
    XEvent event;
  };
};

} // namespace

namespace power_manager {

MockXSync::MockXSync()
    : last_activity_time_(base::Time::Now()),
      event_handler_func(NULL),
      event_handler_data_(NULL) {}

bool MockXSync::QueryExtension(int* event_base, int* error_base) {
  if (event_base)
    *event_base = kEventBase;
  if (error_base)
    *error_base = kErrorBase;
  return true;
}

void MockXSync::SetEventHandler(GdkFilterFunc func, gpointer data) {
  event_handler_func = func;
  event_handler_data_ = data;
}

gboolean MockXSync::TimeoutHandler(MockAlarm *alarm) {
  CHECK(alarm);
  int64 current_idle_time;
  CHECK(QueryCounterInt64(alarm->counter, &current_idle_time));

  // Make sure the amount of idle time requested has indeed been reached, if
  // this is a positive transition alarm.
  if (alarm->positive_transition)
    CHECK(current_idle_time >= alarm->idle_time);

  // Invoke the event handler callback if one has been provided.
  if (event_handler_func) {
    // Create and initialize a new X event object.
    MockGdkXEvent gxevent;
    XEvent* xevent = &gxevent.event;
    XSyncAlarmNotifyEvent* alarm_event = &gxevent.alarm_event;
    xevent->type = kEventBase + XSyncAlarmNotify;
    alarm_event->state = XSyncAlarmActive;
    XSync::Int64ToValue(&alarm_event->counter_value, current_idle_time);
    XSync::Int64ToValue(&alarm_event->alarm_value, alarm->idle_time);
    // Call the event handler to simulate what happens during a gdk event.
    event_handler_func(&gxevent, NULL, event_handler_data_);
  } else {
    LOG(WARNING) << "No event handler callback specified.";
  }
  // Release the alarm handle.
  DestroyMockAlarm(alarm);
  return false;
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
  int64 wait_value = XSync::ValueToInt64(attrs->trigger.wait_value);
  // The idle value for a negative transition must be positive, otherwise it is
  // impossible to attain.  Idle time cannot become negative.
  if (test_type == XSyncNegativeTransition)
    CHECK(wait_value > 0);
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

  if (test_type == XSyncPositiveTransition) {
    alarm->positive_transition = true;
    // Positive transition means idle time exceeds given wait value.
    if (current_idle_time < wait_value) {
      // Create a timeout now.  Assume there will be no further user input.
      // TODO(sque): That assumption is based on current xidle unit test usage.
      // Update this if it changes.
      TimeoutHandlerArgs* args = CreateTimeoutHandlerArgs(this, alarm);
      CHECK(args);
      alarm->ref_count += 1;

      // For some reason, the timeout happens a millisecond sooner than expected
      // so increase the timeout interval to compensate.
      g_timeout_add(wait_value - current_idle_time + 1, &TimeoutHandlerThunk,
                    args);
    } else {
      // Event handler creates timeout when user goes active.
      // TODO(sque): Not used in XIdle unit tests, so not implemented for now.
    }
  } else { // if (test_type == XSyncNegativeTransition)
    // Negative transition means it is triggered when idle time goes below the
    // wait value.
    alarm->positive_transition = false;
    // There is no need to create a timeout event for a negative transition.
    // The only way a decrease in idle time can happen is if the system comes
    // out of idle.  That is done in FakeRelativeMotionEvent().
  }
  // Save this alarm locally until it is deleted.
  alarms_.insert(alarm);

  // TODO(sque): is this legit?  XSyncAlarm is just an integer handle (XID) I
  // believe, but there's no guarantee that it will always be.
  return reinterpret_cast<XSyncAlarm>(alarm);
}

bool MockXSync::DestroyAlarm(XSyncAlarm alarm) {
  return DestroyMockAlarm(reinterpret_cast<MockAlarm*>(alarm));
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
    strcpy(counters[i].name, name);
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
  *value = (base::Time::Now() - last_activity_time_).InMilliseconds();
  return true;
}

bool MockXSync::QueryCounter(XSyncCounter counter, XSyncValue* value) {
  int64 int_value;
  if (!QueryCounterInt64(counter, &int_value))
    return false;
  CHECK(value);
  XSync::Int64ToValue(value, int_value);
  return true;
}

void MockXSync::FakeRelativeMotionEvent(int, int, uint64) {
  // Store the current idle time before coming out of idle.
  int64 idle_time;
  CHECK(QueryCounterInt64(kIdleCounter, &idle_time));
  // Simulate an activity at the current time by resetting the time of last
  // activity to the current time.
  last_activity_time_ = base::Time::Now();

  // Since this brings the system out of idle, find and handle all negative
  // transition alarms.
  AlarmSet::const_iterator iter;
  for (iter = alarms_.begin(); iter != alarms_.end(); ++iter) {
    MockAlarm* alarm = *iter;
    CHECK(alarm);
    // Disregard all the positive transition alarms.
    if (alarm->positive_transition)
      continue;
    // Invoke the alarm if the idle time had at least as long as the alarm's
    // required idle time.
    if (idle_time >= alarm->idle_time)
      TimeoutHandler(alarm);
  }
}

}  // namespace power_manager
