// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_PLATFORM_POWER_MANAGER_XIDLE_H_
#define SRC_PLATFORM_POWER_MANAGER_XIDLE_H_

#include <X11/extensions/scrnsaver.h>
#include <X11/Xlib.h>
#include "base/basictypes.h"

namespace chromeos {

// \brief Connect to the local X Server and determine how long the user has
// been idle, in milliseconds.
//
// Creating a new XIdle object connects to the local X Server. When the object
// is destroyed, the connection is also destroyed.
//
// \example
// chromeos::XIdle idle;
// uint64 idleTime;
// if (idle.getIdleTime(&idleTime))
//   std::cout << "User has been idle for " << idleTime << " milliseconds\n";
// \end_example

class XIdle {
 public:
  XIdle();
  ~XIdle();

  // Return how long the X Server has been idle, in milliseconds.
  // If success, return true; otherwise return false.
  bool getIdleTime(uint64 *idle);
 private:
  Display *display_;
  XScreenSaverInfo *info_;
  int event_base_, error_base_;
  bool valid_;
  DISALLOW_COPY_AND_ASSIGN(XIdle);
};

}  // namespace chromeos

#endif  // SRC_PLATFORM_POWER_MANAGER_XIDLE_H_
