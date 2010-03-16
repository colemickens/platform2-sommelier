// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/xidle.h"
#include <inttypes.h>
#include "base/logging.h"

namespace power_manager {

static inline int64 XSyncValueToInt64(XSyncValue value) {
  return (static_cast<int64>(XSyncValueHigh32(value)) << 32 |
          static_cast<int64>(XSyncValueLow32(value)));
}

static inline void XSyncInt64ToValue(XSyncValue* xvalue, int64 value) {
  XSyncIntsToValue(xvalue, value, value >> 32);
}

XIdle::XIdle()
    : idle_counter_(0),
      min_timeout_(kint64max),
      display_(NULL) {
}

XIdle::~XIdle() {
  ClearTimeouts();
}

bool XIdle::Init(Display* display) {
  int major_version, minor_version;
  CHECK(display);
  display_ = display;
  if (XSyncQueryExtension(display_, &event_base_, &error_base_) &&
      XSyncInitialize(display_, &major_version, &minor_version)) {
    XSyncSystemCounter* counters;
    int ncounters;
    counters = XSyncListSystemCounters(display_, &ncounters);

    if (counters) {
      for (int i = 0; i < ncounters; i++) {
        if (counters[i].name && strcmp(counters[i].name, "IDLETIME") == 0) {
          idle_counter_ = counters[i].counter;
          break;
        }
      }
      XSyncFreeSystemCounterList(counters);
    }
  }
  return idle_counter_ != 0;
}

bool XIdle::AddIdleTimeout(int64 idle_timeout_ms) {
  DCHECK_NE(idle_counter_, 0);
  DCHECK_GT(idle_timeout_ms, 1);

  if (idle_timeout_ms < min_timeout_) {
    min_timeout_ = idle_timeout_ms;

    // Setup an alarm to fire when the user was idle, but is now active.
    // This occurs when old_idle_time > min_timeout_ - 1, and the user becomes
    // active.
    XSyncAlarm alarm = CreateIdleAlarm(min_timeout_ - 1,
                                       XSyncNegativeTransition);
    if (!alarm)
      return false;
    if (!alarms_.empty()) {
      XSyncDestroyAlarm(display_, alarms_.front());
      alarms_.pop_front();
    }
    alarms_.push_front(alarm);
  }

  // Send idle event when new_idle_time >= idle_timeout_ms
  XSyncAlarm alarm = CreateIdleAlarm(idle_timeout_ms, XSyncPositiveTransition);
  if (alarm)
    alarms_.push_back(alarm);
  return alarm != 0;
}

bool XIdle::Monitor(bool *is_idle, int64* idle_time_ms) {
  DCHECK_NE(idle_counter_, 0);
  CHECK(!alarms_.empty());

  XEvent event;
  XSyncAlarmNotifyEvent* alarm_event =
      reinterpret_cast<XSyncAlarmNotifyEvent*>(&event);

  while (XNextEvent(display_, &event) == 0) {
    if (event.type == event_base_ + XSyncAlarmNotify &&
        alarm_event->state != XSyncAlarmDestroyed) {
      *is_idle = !XSyncValueLessThan(alarm_event->counter_value,
                                     alarm_event->alarm_value);
      *idle_time_ms = XSyncValueToInt64(alarm_event->counter_value);
      return true;
    }
  }

  return false;
}

bool XIdle::GetIdleTime(int64* idle_time_ms) {
  DCHECK_NE(idle_counter_, 0);
  XSyncValue value;
  if (XSyncQueryCounter(display_, idle_counter_, &value)) {
    *idle_time_ms = XSyncValueToInt64(value);
    return true;
  }
  return false;
}

bool XIdle::ClearTimeouts() {
  std::list<XSyncAlarm>::iterator it = alarms_.begin();
  for (; it != alarms_.end(); it++) {
    XSyncDestroyAlarm(display_, *it);
  }
  alarms_.clear();
  min_timeout_ = kint64max;
}

XSyncAlarm XIdle::CreateIdleAlarm(int64 idle_timeout_ms,
                                  XSyncTestType test_type) {
  uint64 mask = XSyncCACounter |
                XSyncCAValue |
                XSyncCATestType |
                XSyncCADelta;
  XSyncAlarmAttributes attr;
  attr.trigger.counter = idle_counter_;
  attr.trigger.test_type = test_type;
  XSyncInt64ToValue(&attr.trigger.wait_value, idle_timeout_ms);
  XSyncIntToValue(&attr.delta, 0);
  return XSyncCreateAlarm(display_, mask, &attr);
}

}  // namespace power_manager
