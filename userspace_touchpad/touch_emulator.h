// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USERSPACE_TOUCHPAD_TOUCH_EMULATOR_H_
#define USERSPACE_TOUCHPAD_TOUCH_EMULATOR_H_

#include <stdint.h>
#include <vector>

#include "userspace_touchpad/uinputdevice.h"

// Maximum number of simutenously active fingers.
constexpr size_t kMaxFingers = 10;

// Finger touch state/event struct.
struct TouchEvent {
  int x;
  int y;
  int pressure;
  int tracking_id;
};

// Linux multi-touch device emulator class.
class TouchEmulator {
 public:
  TouchEmulator();
  ~TouchEmulator() {}

  // Write device events for the current set of fingers.
  void FlushEvents(const std::vector<TouchEvent>& fingers,
                   const bool button_down);

 private:
  // Creates a virtual multi device for surfacing touch events.
  void CreateVirtualMultiTouchDevice();

  // Writes touch events for a finger to the virtual MT device.
  void WriteTouchEvent(int slot, int id, int x, int y, int pressure);

  // Writes MT-related button events to the virtual MT device.
  void WriteTouchButtonEvent(int finger_count);

  // Set up the range and resolution for a given EV_ABS event code
  void AbsInit(int code, int min_val, int max_val, int res);

  // The virtual multi-touch device handler.
  UinputDevice touch_device_handler_;

  // The tracking ids of fingers in each slot. -1 if there is no finger in one.
  int slot_tid_[kMaxFingers];
};

#endif  // USERSPACE_TOUCHPAD_TOUCH_EMULATOR_H_
