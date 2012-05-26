// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_IDLE_H_
#define POWER_MANAGER_IDLE_H_

#include <set>

#include "base/basictypes.h"
#include "base/time.h"
#include "power_manager/signal_callback.h"

typedef int gint;
typedef gint gboolean;

namespace power_manager {

// Interface for observing idle events.
class IdleObserver {
 public:
  // Idle event handler. This handler should be called when the user either
  // becomes newly idle (due to exceeding an idle timeout) or is no longer
  // idle.
  virtual void OnIdleEvent(bool is_idle, int64 idle_time_ms) = 0;

 protected:
  virtual ~IdleObserver() {}
};

// glib-based idle event detector.
class IdleDetector {
 public:
  IdleDetector();
  ~IdleDetector();

  // Initialize the object with the given |observer|.
  void Init(IdleObserver* observer);

  // Add a timeout value. Idle events will be fired every time
  // the user either becomes newly idle (due to exceeding an idle
  // timeout) or is no longer idle.
  void AddIdleTimeout(int64 idle_timeout_ms);

  // Returns how long the user has been idle, in milliseconds.
  int64 GetIdleTimeMs() const;

  // Clear all timeouts.
  void ClearTimeouts();

  // Resets the |last_activity_time_| to the current time.
  // Should be called whenever there is user input activity.
  void HandleUserActivity(const base::TimeTicks& last_activity_time);

 private:
  struct Alarm {
    int64 timeout_ms;
    gint source_id;
  };

  // Create an alarm with the given timeout value.
  void CreateAlarm(int64 idle_timeout_ms);

  // Deletes an alarm created by CreateAlarm().
  void DeleteAlarm(Alarm* alarm);

  // Reset alarm timeouts, called when there is user activity.
  void ResetAlarms();

  // Callback for idle alarms.
  SIGNAL_CALLBACK_PACKED_1(IdleDetector, gboolean, HandleAlarm, Alarm*);

  // The last time user activity was registered.
  base::TimeTicks last_activity_time_;

  // Non-owned pointer to object listening for changes to idle state.
  IdleObserver* observer_;

  // Set when an idle alarm has been triggered.
  // Cleared when user activity ends idle state.
  bool is_idle_;

  // A list of the alarms.
  std::set<Alarm*> alarms_;

  DISALLOW_COPY_AND_ASSIGN(IdleDetector);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_IDLE_H_
