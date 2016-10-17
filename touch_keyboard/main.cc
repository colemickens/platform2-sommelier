// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>
#include <sys/wait.h>
#include "touch_keyboard/faketouchpad.h"

// This filepath is used as the input evdev device. Whichever touch sensor is
// to be used for touch keyboard input should have a udev rule put in place to
// set up this symlink.
constexpr char kTouchSensorDevicePath[] = "/dev/touch_keyboard";

using touch_keyboard::FakeTouchpad;

int main() {
  LOG(INFO) << "Starting touch_keyboard_handler";

  // TODO(charliemooney): Get these coordinates from somewhere not hard-coded.
  LOG(INFO) << "Creating Fake Touchpad.";
  FakeTouchpad tp = FakeTouchpad(755, 1765, 35, 585, touch_keyboard::kInvertY);
  tp.Start(kTouchSensorDevicePath, "virtual-touchpad");

  return 0;
}
