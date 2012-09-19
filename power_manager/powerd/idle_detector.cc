// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/idle_detector.h"

#include <glib.h>
#include <inttypes.h>

#include "base/logging.h"

namespace power_manager {

IdleDetector::IdleDetector()
    : last_activity_time_(base::TimeTicks::Now()),
      observer_(NULL),
      is_idle_(false) {
}

IdleDetector::~IdleDetector() {
  ClearTimeouts();
}

void IdleDetector::AddObserver(IdleObserver* observer) {
  if (!observer_)
    observer_ = observer;
  else
    LOG(WARNING) << "Attempting to overwrite an existing registered observer.";
}

void IdleDetector::RemoveObserver(IdleObserver* observer) {
  if (observer_ == observer)
    observer_ = NULL;
  else
    LOG(WARNING) << "Observer was not registered with IdleDetector.";
}

void IdleDetector::AddIdleTimeout(int64 idle_timeout_ms) {
  DCHECK_GT(idle_timeout_ms, 0);
  // Send idle event when new_idle_time >= idle_timeout_ms
  CreateAlarm(idle_timeout_ms);
}

int64 IdleDetector::GetIdleTimeMs() const {
  return (base::TimeTicks::Now() - last_activity_time_).InMilliseconds();
}

void IdleDetector::ClearTimeouts() {
  while (!alarms_.empty())
    DeleteAlarm(*alarms_.begin());
}

void IdleDetector::HandleUserActivity(
    const base::TimeTicks& last_activity_time) {
  last_activity_time_ = last_activity_time;
  ResetAlarms();
  // Handle this coming-out-of-idle event.
  if (is_idle_ && observer_)
    observer_->OnIdleEvent(false, GetIdleTimeMs());
  is_idle_ = false;
}

void IdleDetector::CreateAlarm(int64 idle_timeout_ms) {
  Alarm* alarm = new Alarm;
  alarm->timeout_ms = idle_timeout_ms;
  alarm->source_id = 0;

  int64 current_idle_time_ms = GetIdleTimeMs();
  if (current_idle_time_ms < alarm->timeout_ms) {
    alarm->source_id = g_timeout_add(alarm->timeout_ms - current_idle_time_ms,
                                     HandleAlarmThunk,
                                     CreateHandleAlarmArgs(this, alarm));
  }
  CHECK(alarms_.find(alarm) == alarms_.end());
  alarms_.insert(alarm);
}

void IdleDetector::DeleteAlarm(Alarm* alarm) {
  if (alarm->source_id > 0)
    g_source_remove(alarm->source_id);
  CHECK(alarms_.find(alarm) != alarms_.end());
  alarms_.erase(alarm);
  delete alarm;
}

void IdleDetector::ResetAlarms() {
  std::set<Alarm*>::iterator iter;
  for (iter = alarms_.begin(); iter != alarms_.end(); ++iter) {
    Alarm* alarm = *iter;
    if (alarm->source_id)
      g_source_remove(alarm->source_id);
    alarm->source_id = g_timeout_add(alarm->timeout_ms, HandleAlarmThunk,
                                     CreateHandleAlarmArgs(this, alarm));
  }
}

gboolean IdleDetector::HandleAlarm(Alarm* alarm) {
  is_idle_ = true;
  // Clear the source tag to show that this alarm has having been triggered.
  alarm->source_id = 0;
  // Invoke the idle event observer callback last, because it could possibly
  // call ClearTimeouts() and delete |alarm|.
  if (observer_)
    observer_->OnIdleEvent(true, GetIdleTimeMs());
  return FALSE;
}

}  // namespace power_manager
