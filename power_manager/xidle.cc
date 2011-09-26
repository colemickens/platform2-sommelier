// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/xidle.h"

#include <gdk/gdkx.h>
#include <inttypes.h>

#include "base/logging.h"
#include "power_manager/xidle_observer.h"

namespace power_manager {

XIdle::XIdle()
    : xsync_(new XSync()),
      idle_counter_(0),
      min_timeout_(kint64max) {
}

XIdle::XIdle(XSync *xsync)
    : xsync_(xsync),
      idle_counter_(0),
      min_timeout_(kint64max) {
}

XIdle::~XIdle() {
  ClearTimeouts();
}

bool XIdle::Init(XIdleObserver* observer) {
  xsync_->Init();
  CHECK(xsync_.get());
  if (!xsync_->QueryExtension(&event_base_, &error_base_)) {
    LOG(WARNING) << "Error querying XSync extension.";
    return false;
  }
  int major_version = -1;
  int minor_version = -1;
  if (!xsync_->Initialize(&major_version, &minor_version)) {
    if (major_version == -1 && minor_version == -1)
      LOG(WARNING) << "Unable to read XSync version.";
    else
      LOG(WARNING) << "Invalid XSync version: "
                   << major_version << "." << minor_version;
    return false;
  }

  XSyncSystemCounter* counters;
  int ncounters;
  counters = xsync_->ListSystemCounters(&ncounters);
  if (counters) {
    for (int i = 0; i < ncounters; ++i) {
      if (counters[i].name && strcmp(counters[i].name, "IDLETIME") == 0) {
        idle_counter_ = counters[i].counter;
        break;
      }
    }
    xsync_->FreeSystemCounterList(counters);
    if (idle_counter_ && observer) {
      observer_ = observer;
      xsync_->SetEventHandler(GdkEventFilterThunk, this);
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
      xsync_->DestroyAlarm(alarms_.front());
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

bool XIdle::GetIdleTime(int64* idle_time_ms) {
  DCHECK_NE(idle_counter_, 0);
  return xsync_->QueryCounterInt64(idle_counter_, idle_time_ms);
}

bool XIdle::ClearTimeouts() {
  std::list<XSyncAlarm>::iterator it;
  for (it = alarms_.begin(); it != alarms_.end(); ++it)
    xsync_->DestroyAlarm(*it);
  alarms_.clear();
  min_timeout_ = kint64max;
  return true;
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
  XSync::Int64ToValue(&attr.trigger.wait_value, idle_timeout_ms);
  XSyncIntToValue(&attr.delta, 0);
  return xsync_->CreateAlarm(mask, &attr);
}

GdkFilterReturn XIdle::GdkEventFilter(GdkXEvent* gxevent, GdkEvent*) {
  XEvent* xevent = static_cast<XEvent*>(gxevent);
  XSyncAlarmNotifyEvent* alarm_event =
      static_cast<XSyncAlarmNotifyEvent*>(gxevent);
  CHECK(idle_counter_);
  CHECK(!alarms_.empty());

  XSyncValue value;
  if (xevent->type == event_base_ + XSyncAlarmNotify &&
      alarm_event->state != XSyncAlarmDestroyed &&
      xsync_->QueryCounter(idle_counter_, &value)) {
    bool is_idle = !XSyncValueLessThan(alarm_event->counter_value,
                                       alarm_event->alarm_value);
    bool is_idle2 = !XSyncValueLessThan(value, alarm_event->alarm_value);
    if (is_idle == is_idle2) {
      int64 idle_time_ms = XSync::ValueToInt64(alarm_event->counter_value);
      observer_->OnIdleEvent(is_idle, idle_time_ms);
    } else {
      LOG(INFO) << "Filtering out stale event";
    }
  }

  return GDK_FILTER_CONTINUE;
}

}  // namespace power_manager
