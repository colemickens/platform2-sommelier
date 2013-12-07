// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_INPUT_H_
#define POWER_MANAGER_POWERD_SYSTEM_INPUT_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/file_path.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/observer_list.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/powerd/system/input_interface.h"
#include "power_manager/powerd/system/udev_observer.h"

namespace power_manager {

class PrefsInterface;

namespace system {

class InputObserver;
class UdevInterface;

class Input : public InputInterface,
              public MessageLoopForIO::Watcher,
              public UdevObserver {
 public:
  // udev subsystem to watch for input device-related events.
  static const char kInputUdevSubsystem[];

  // Filename within a DRM device directory containing the device's hotplug
  // status.
  static const char kDrmStatusFile[];

  // Value in |kDrmStatusFile| indicating that the device is connected.
  static const char kDrmStatusConnected[];

  Input();
  virtual ~Input();

  void set_sysfs_input_path_for_testing(const base::FilePath& path) {
    sysfs_input_path_for_testing_ = path;
  }
  void set_sysfs_drm_path_for_testing(const base::FilePath& path) {
    sysfs_drm_path_for_testing_ = path;
  }

  // Returns true on success.
  bool Init(PrefsInterface* prefs, UdevInterface* udev);

  // InputInterface implementation:
  virtual void AddObserver(InputObserver* observer) OVERRIDE;
  virtual void RemoveObserver(InputObserver* observer) OVERRIDE;
  virtual LidState QueryLidState() OVERRIDE;
  virtual bool IsUSBInputDeviceConnected() const OVERRIDE;
  virtual bool IsDisplayConnected() const OVERRIDE;
  virtual int GetActiveVT() OVERRIDE;
  virtual bool SetWakeInputsState(bool enable) OVERRIDE;
  virtual void SetTouchDevicesState(bool enable) OVERRIDE;

  // MessageLoopForIO::Watcher implementation:
  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE;
  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE;

  // UdevObserver implementation:
  virtual void OnUdevEvent(const std::string& subsystem,
                           const std::string& sysname,
                           UdevObserver::Action action) OVERRIDE;

 private:
  class EventFileDescriptor;

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

  // Adds or removes events to handle lid and power button.
  bool AddEvent(const std::string& name);
  bool RemoveEvent(const std::string& name);

  // Adds or removes inputs used for enabling and disabling wakeup events.
  bool AddWakeInput(const std::string& name);
  bool RemoveWakeInput(const std::string& name);

  // Starts watching |fd| for events if it corresponds to a power button or lid
  // switch. Takes ownership of |fd| and returns true if the descriptor is now
  // watched; the caller is responsible for closing |fd| otherwise.
  bool RegisterInputEvent(int fd, int event_num);

  // File descriptor corresponding to the lid switch. The EventFileDescriptor
  // in |registered_inputs_| handles closing this FD; it's stored separately so
  // it can be queried directly for the lid state.
  int lid_fd_;

  int num_power_key_events_;
  int num_lid_events_;

  bool wakeups_enabled_;

  // Should the lid be watched for events if present?
  bool use_lid_;

  // Name of the power button interface to skip monitoring.
  const char* power_button_to_skip_;

  // Used to make ioctls to /dev/console to check which VT is active.
  int console_fd_;

  UdevInterface* udev_;  // non-owned

  // Keyed by input event number.
  typedef std::map<int, linked_ptr<EventFileDescriptor> > InputMap;
  InputMap registered_inputs_;

  // Maps from an input name to an input number.
  typedef std::map<std::string, int> WakeupMap;
  WakeupMap wakeup_inputs_map_;

  ObserverList<InputObserver> observers_;

  // Used by IsUSBInputDeviceConnected() instead of the default path if
  // non-empty.
  base::FilePath sysfs_input_path_for_testing_;

  // Used by IsDisplayConnected() instead of the default path if non-empty.
  base::FilePath sysfs_drm_path_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(Input);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_INPUT_H_
