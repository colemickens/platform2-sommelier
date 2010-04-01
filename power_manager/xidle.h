// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_XIDLE_H_
#define POWER_MANAGER_XIDLE_H_

#include <gdk/gdkevents.h>
#include <X11/Xlib.h>
#include <X11/extensions/sync.h>
#include <list>
#include "base/basictypes.h"
#include "power_manager/xidle_monitor.h"

namespace power_manager {

// \brief Receive notifications from the X Server when the user is marked as
// idle, or as no longer idle.
//
// \example
// class IdleMonitorExample : public power_manager::XIdleMonitor {
//  public:
//   void OnIdleEvent(bool is_idle, int64 idle_time_ms) {
//     if (is_idle)
//       printf("User has been idle for %" PRIi64 " ms\n", idle_time_ms);
//     else
//       printf("User is active\n");
//   }
// };
// IdleMonitorExample example;
// power_manager::XIdle idle;
// idle.Init(&example);
// idle.AddIdleTimeout(2000);
// idle.AddIdleTimeout(5000);
// GMainLoop* loop = g_main_loop_new(NULL, false);
// g_main_loop_run(loop);
// \end_example

class XIdle {
 public:
  XIdle();
  ~XIdle();

  // Initialize the object with the given monitor.
  // On success, return true; otherwise return false.
  bool Init(XIdleMonitor* monitor);

  // Add a timeout value. Idle events will be fired every time
  // the user either becomes newly idle (due to exceeding an idle
  // timeout) or is no longer idle.
  //
  // On success, return true; otherwise return false.
  bool AddIdleTimeout(int64 idle_timeout_ms);

  // Set *idle_time_ms to how long the user has been idle, in milliseconds.
  // On success, return true; otherwise return false.
  bool GetIdleTime(int64* idle_time_ms);

  // Clear all timeouts.
  // On success, return true; otherwise return false.
  bool ClearTimeouts();

 private:

  // Create an XSyncAlarm. Returns the new alarm.
  //
  // If test_type is XSyncPositiveTransition, the alarm triggers when the
  // idle timeout is exceeded. On the other hand, if test_type is
  // XSyncNegativeTransition, the alarm triggers when the user is no longer
  // idle.
  XSyncAlarm CreateIdleAlarm(int64 idle_timeout_ms, XSyncTestType test_type);

  static GdkFilterReturn gdk_event_filter(GdkXEvent* gxevent,
    GdkEvent* gevent, gpointer data);

  XSyncCounter idle_counter_;
  int64 min_timeout_;
  int event_base_, error_base_;

  // Non-owned pointer to object listening for changes to idle state
  XIdleMonitor* monitor_;

  // A list of the alarms. If non-empty, the negative transition alarm for
  // min_timeout_ will be the first alarm in the list.
  std::list<XSyncAlarm> alarms_;

  DISALLOW_COPY_AND_ASSIGN(XIdle);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_XIDLE_H_
