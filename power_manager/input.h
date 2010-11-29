// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_INPUT_H_
#define POWER_MANAGER_INPUT_H_

#include <glib.h>

#include "base/basictypes.h"

namespace power_manager {

enum InputType {
 LID,
 PWRBUTTON,
};

class Input {
 public:
  Input();
  ~Input();

  // Initialize the input object.
  //
  // On success, return true; otherwise return false.
  bool Init();

  // Input handler function type. |data| is object passed to RegisterHandler.
  // |type| is InputType of this event. |value| is the new state of this input
  // device.
  typedef void (*InputHandler)(void* data, InputType type, int value);
  void RegisterHandler(InputHandler handler,
                       void* data);

  // |lid_state| is 1 for closed lid. 0 for opened lid.
  bool QueryLidState(int* lid_state);

  int num_lid_events() const {
    return num_lid_events_;
  }
  int num_power_key_events() const {
    return num_power_key_events_;
  }

 private:
  // For every "event" in /dev/input/, open a file handle, and
  // RegisterInputEvent on it.
  bool RegisterInputDevices();

  // Check that the event of handle |fd| advertises power key or lid event.
  // If so, watch this event for changes.
  bool RegisterInputEvent(int fd);

  // Event handler for input events. |source| contains the IO Channel that has
  // changed. |condition| contains the condition of change. |data| contains a
  // pointer to an Input object.
  static gboolean EventHandler(GIOChannel* source,
                               GIOCondition condition,
                               gpointer data);

  // Daemon's handler, and data
  InputHandler handler_;
  void* handler_data_;
  int lid_fd_;
  int num_power_key_events_;
  int num_lid_events_;

  DISALLOW_COPY_AND_ASSIGN(Input);
};

} // namespace power_manager

#endif // POWER_MANAGER_INPUT_H_
