// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_INPUT_WATCHER_H_
#define POWER_MANAGER_POWERD_SYSTEM_INPUT_WATCHER_H_

#include <map>
#include <string>
#include <vector>

#include <base/cancelable_callback.h>
#include <base/files/file_path.h>
#include <base/macros.h>
#include <base/memory/linked_ptr.h>
#include <base/observer_list.h>

#include "power_manager/common/power_constants.h"
#include "power_manager/powerd/system/input_watcher_interface.h"
#include "power_manager/powerd/system/udev_subsystem_observer.h"

struct input_event;  // from <linux/input.h>

namespace power_manager {

class PrefsInterface;

namespace system {

class EventDeviceInterface;
class EventDeviceFactoryInterface;
class InputObserver;
class UdevInterface;

class InputWatcher : public InputWatcherInterface,
                     public UdevSubsystemObserver {
 public:
  // udev subsystem to watch for input device-related events.
  static const char kInputUdevSubsystem[];

  InputWatcher();
  virtual ~InputWatcher();

  void set_sysfs_input_path_for_testing(const base::FilePath& path) {
    sysfs_input_path_for_testing_ = path;
  }

  // Returns true on success.
  bool Init(scoped_ptr<EventDeviceFactoryInterface> event_device_factory,
            PrefsInterface* prefs,
            UdevInterface* udev);

  // InputWatcherInterface implementation:
  void AddObserver(InputObserver* observer) override;
  void RemoveObserver(InputObserver* observer) override;
  LidState QueryLidState() override;
  bool IsUSBInputDeviceConnected() const override;
  int GetActiveVT() override;

  // UdevSubsystemObserver implementation:
  void OnUdevEvent(const std::string& subsystem,
                   const std::string& sysname,
                   UdevAction action) override;

 private:
  // Flushes queued events and reads new events from |device|.
  void OnNewEvents(EventDeviceInterface* device);

  // Updates internal state and notifies observers in response to |event|.
  void ProcessEvent(const input_event& event);

  // Updates |current_multitouch_slot_| and |multitouch_slots_hover_state_| in
  // response to |event|, doing nothing if it isn't actually an MT event. Helper
  // method called by ProcessEvent().
  void ProcessHoverEvent(const input_event& event);

  // Handles an input being added to or removed from the system.
  void HandleAddedInput(const std::string& input_name, int input_num);
  void HandleRemovedInput(int input_num);

  // Calls NotifyObserversAboutEvent() for each event in |queued_events_| and
  // clears the vector.
  void SendQueuedEvents();

  // Notifies observers about |event| if came from a lid switch or power button.
  void NotifyObserversAboutEvent(const input_event& event);

  // Factory to access EventDevices.
  scoped_ptr<EventDeviceFactoryInterface> event_device_factory_;

  // The event device exposing the lid switch. Weak pointer to an element in
  // |event_devices_|, or NULL if no lid device was found.
  EventDeviceInterface* lid_device_;

  // The event device reporting hover events. Weak pointer to an element in
  // |event_devices_|, or NULL if no hover device was found.
  EventDeviceInterface* hover_device_;

  // Should the lid be watched for events if present?
  bool use_lid_;

  // Most-recently-seen lid state.
  LidState lid_state_;

  // Should hover events be reported?
  bool detect_hover_;

  // Most-recently-reported hover state.
  bool hovering_;

  // Multitouch slot for which input events are currently being reported. See
  // https://www.kernel.org/doc/Documentation/input/multi-touch-protocol.txt for
  // more details about the protocol.
  int current_multitouch_slot_;

  // Bitmap containing the hover state of individual multitouch slots; if a bit
  // is true, it indicates that the corresponding slot is either reporting a
  // hover event above the touchpad or a touch event on the touchpad.
  uint64_t multitouch_slots_hover_state_;

  // Events read from |lid_device_| by QueryLidState() that haven't yet been
  // sent to observers.
  std::vector<input_event> queued_events_;

  // Posted by QueryLidState() to run SendQueuedEvents() to notify observers
  // about |queued_events_|.
  base::CancelableClosure send_queued_events_task_;

  // Name of the power button interface to skip monitoring.
  const char* power_button_to_skip_;

  // Used to make ioctls to /dev/console to check which VT is active.
  int console_fd_;

  UdevInterface* udev_;  // non-owned

  // Keyed by input event number.
  typedef std::map<int, linked_ptr<EventDeviceInterface>> InputMap;
  InputMap event_devices_;

  ObserverList<InputObserver> observers_;

  // Used by IsUSBInputDeviceConnected() instead of the default path if
  // non-empty.
  base::FilePath sysfs_input_path_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(InputWatcher);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_INPUT_WATCHER_H_
