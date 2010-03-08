// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/xidle.h"
#include <X11/extensions/scrnsaver.h>
#include <X11/Xlib.h>

namespace chromeos {

XIdle::XIdle() {
  display_ = XOpenDisplay(NULL);
  info_ = XScreenSaverAllocInfo();
  valid_ = display_ &&
           XScreenSaverQueryExtension(display_, &event_base_, &error_base_);
}

XIdle::~XIdle() {
  if (info_)
    XFree(info_);
  if (display_)
    XCloseDisplay(display_);
}

bool XIdle::getIdleTime(uint64 *idle) {
  if (valid_ &&
      XScreenSaverQueryInfo(display_, DefaultRootWindow(display_), info_)) {
    *idle = info_->idle;
    return true;
  }
  return false;
}

}  // namespace chromeos
