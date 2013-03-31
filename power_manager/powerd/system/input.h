// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_INPUT_H_
#define POWER_MANAGER_POWERD_SYSTEM_INPUT_H_

#include <glib.h>

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/observer_list.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/common/signal_callback.h"

// Forward declarations of structs from libudev.h.
struct udev;
struct udev_monitor;

namespace power_manager {
namespace system {

class InputObserver;

class Input {
 public:
  Input();
  ~Input();

  void set_sysfs_input_path_for_testing(const std::string& path) {
    sysfs_input_path_for_testing_ = path;
  }

  // |wakeup_inputs_names| contains input device names that may wake the
  // system from resume.  If |use_lid| is true, the lid will be watched for
  // events if present.  Returns true on success.
  bool Init(const std::vector<std::string>& wakeup_input_names, bool use_lid);

  // Adds or removes an observer.
  void AddObserver(InputObserver* observer);
  void RemoveObserver(InputObserver* observer);

  // Queries the system for the current lid state.  LID_NOT_PRESENT is
  // returned on error.
  LidState QueryLidState();

  // Checks if any USB input devices are connected, by scanning sysfs for input
  // devices whose paths contain "usb".
  bool IsUSBInputDeviceConnected() const;

  // Returns the (1-indexed) number of the currently-active virtual terminal.
  int GetActiveVT();

  // Enable or disable special wakeup input devices.
  bool SetWakeInputsState(bool enable);

  // Enable or disable touch devices.
  void SetTouchDevicesState(bool enable);

  int num_lid_events() const {
    return num_lid_events_;
  }
  int num_power_key_events() const {
    return num_power_key_events_;
  }

 private:
  struct IOChannelWatch {
    IOChannelWatch() : channel(NULL), source_id(0) {}
    IOChannelWatch(GIOChannel* channel, guint source_id)
        : channel(channel),
          source_id(source_id) {
    }

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

  SIGNAL_CALLBACK_2(Input, gboolean, HandleUdevEvent, GIOChannel*,
                    GIOCondition);

  void RegisterUdevEventHandler();

  // Event handler for input events. |source| contains the IO Channel that has
  // changed. |condition| contains the condition of change. |data| contains a
  // pointer to an Input object.
  SIGNAL_CALLBACK_2(Input, gboolean, HandleInputEvent, GIOChannel*,
                    GIOCondition);

  int lid_fd_;
  int num_power_key_events_;
  int num_lid_events_;
  struct udev_monitor* udev_monitor_;
  struct udev* udev_;
  bool wakeups_enabled_;

  // Should the lid be watched for events if present?
  bool use_lid_;

  // Used to make ioctls to /dev/console to check which VT is active.
  int console_fd_;

  // Maps from an input event number to a source tag.
  typedef std::map<int, IOChannelWatch> InputMap;
  InputMap registered_inputs_;

  // Maps from an input name to an input number.
  typedef std::map<std::string, int> WakeupMap;
  WakeupMap wakeup_inputs_map_;

  ObserverList<InputObserver> observers_;

  // Used by IsUSBInputDeviceConnected() instead of the default input path, if
  // this string is non-empty.  Used for testing purposes.
  std::string sysfs_input_path_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(Input);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_INPUT_H_
