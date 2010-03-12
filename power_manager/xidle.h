// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_PLATFORM_POWER_MANAGER_XIDLE_H_
#define SRC_PLATFORM_POWER_MANAGER_XIDLE_H_

#include <X11/Xlib.h>
#include <X11/extensions/sync.h>
#include <list>
#include "base/basictypes.h"

namespace power_manager {

// \brief Connect to the local X Server and receive notifications when
// the user is marked as idle, or as no longer idle.
//
// Creating a new XIdle object connects to the local X Server. When the object
// is destroyed, the connection is also destroyed.
//
// \example
// power_manager::XIdle idle;
// idle.Init();
// idle.AddIdleTimeout(2000);
// idle.AddIdleTimeout(5000);
// int64 idle_time;
// bool is_idle;
// while (idle.Monitor(&is_idle, &idle_time)) {
//   if (is_idle)
//     std::cout << "User has been idle for " << idle_time << " ms\n";
//   else
//     std::cout << "User is active\n";
// }
// \end_example

class XIdle {
 public:
  XIdle();
  ~XIdle();

  // Connect to the X server and initialize the object.
  // On success, return true; otherwise return false.
  bool Init();

  // Add a timeout value. If the user exceeds this timeout value,
  // the Monitor function will return and notify us. If there are
  // multiple idle timeouts, the Monitor function will return every
  // time each of the idle timeouts is exceeded.
  //
  // On success, return true; otherwise return false.
  bool AddIdleTimeout(int64 idle_timeout_ms);

  // Monitor how long the user has been idle. The function will return
  // whenever the user either becomes newly idle (due to exceeding an
  // idle timeout) or is no longer idle.
  //
  // Sets *is_idle to whether the user is now considered idle.
  // Sets *idle_time_ms to how long the user has been idle, in milliseconds.
  //
  // On success, return true; otherwise return false.
  bool Monitor(bool *is_idle, int64* idle_time_ms);

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

  XSyncCounter idle_counter_;
  int64 min_timeout_;
  Display* display_;
  int event_base_, error_base_;

  // A list of the alarms. If non-empty, the negative transition alarm for
  // min_timeout_ will be the first alarm in the list.
  std::list<XSyncAlarm> alarms_;

  DISALLOW_COPY_AND_ASSIGN(XIdle);
};

}  // namespace power_manager

#endif  // SRC_PLATFORM_POWER_MANAGER_XIDLE_H_
