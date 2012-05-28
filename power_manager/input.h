// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_INPUT_H_
#define POWER_MANAGER_INPUT_H_

#include <glib.h>
#include <libudev.h>

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"

namespace power_manager {

enum InputType {
  INPUT_LID,
  INPUT_POWER_BUTTON,
  INPUT_LOCK_BUTTON,
  INPUT_KEY_LEFT_CTRL,
  INPUT_KEY_RIGHT_CTRL,
  INPUT_KEY_LEFT_ALT,
  INPUT_KEY_RIGHT_ALT,
  INPUT_KEY_LEFT_SHIFT,
  INPUT_KEY_RIGHT_SHIFT,
  INPUT_KEY_F4,
  INPUT_UNHANDLED,
};

class Input {
 public:
  Input();
  ~Input();

  // Initialize the input object.
  // |wakeup_inputs_names| is a vector of strings of input device names that may
  // wake the system from resume that may be disabled.
  // On success, return true; otherwise return false.
  bool Init(const std::vector<std::string>& wakeup_inputs_names);

  // Input handler function type. |data| is object passed to RegisterHandler.
  // |type| is InputType of this event. |value| is the new state of this input
  // device.
  typedef void (*InputHandler)(void* data, InputType type, int value);
  void RegisterHandler(InputHandler handler,
                       void* data);

  // |lid_state| is 1 for closed lid. 0 for opened lid.
  bool QueryLidState(int* lid_state);

  // Disable and Enable special wakeup input devices.
  bool DisableWakeInputs();
  bool EnableWakeInputs();

  int num_lid_events() const {
    return num_lid_events_;
  }
  int num_power_key_events() const {
    return num_power_key_events_;
  }

 private:
  struct IOChannelWatch {
    GIOChannel* channel;
    guint source_id;
  };

  // Closes the G_IO channel |channel| and its file handle.
  void CloseIOChannel(IOChannelWatch* channel);

  // Add/Remove events to handle lid and power button.
  bool AddEvent(const char* name);
  bool RemoveEvent(const char* name);

  // Add/Remove Inputs used for enabling and disabling wakeup events.
  bool AddWakeInput(const char* name);
  bool RemoveWakeInput(const char* name);

  // For every "event" in /dev/input/, open a file handle, and
  // RegisterInputEvent on it if the event contains power buttons or lid.
  bool RegisterInputDevices();

  // For every "input" in /sys/class/input/, keep track of power/wakeup if
  // it matches wake inputs names.
  bool RegisterInputWakeSources();

  // Set power/wakeup for all tracked input devices to wakeups_enabled_
  bool SetInputWakeupStates();
  // Set power/wakeup for input device number |input_num|
  bool SetWakeupState(int input_num, bool enabled);

  // Check that the event of handle |fd| advertises power key or lid event.
  // If so, watch this event for changes.
  // Returns true if the event watch was successfully created.
  // If it returns false, it doesn't necessarily mean there was an error.  The
  // file handle could have been for something other than the power key or lid.
  bool RegisterInputEvent(int fd, int event_num);

  static gboolean UdevEventHandler(GIOChannel* source,
                                   GIOCondition condition,
                                   gpointer data);
  void RegisterUdevEventHandler();

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
  struct udev_monitor* udev_monitor_;
  struct udev* udev_;
  bool wakeups_enabled_;

  // maps from an input event number to a source tag.
  typedef std::map<int, IOChannelWatch> InputMap;
  InputMap registered_inputs_;

  // maps from an input name to an input number
  typedef std::map<std::string, int> WakeupMap;
  WakeupMap wakeup_inputs_map_;

  DISALLOW_COPY_AND_ASSIGN(Input);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_INPUT_H_
