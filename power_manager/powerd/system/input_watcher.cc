// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/input_watcher.h"

#include <fcntl.h>
#include <linux/input.h>
#include <linux/vt.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <base/callback.h>
#include <base/files/file_enumerator.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/posix/eintr_wrapper.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>

#include "power_manager/common/power_constants.h"
#include "power_manager/common/prefs.h"
#include "power_manager/common/util.h"
#include "power_manager/powerd/system/acpi_wakeup_helper.h"
#include "power_manager/powerd/system/event_device_interface.h"
#include "power_manager/powerd/system/input_observer.h"
#include "power_manager/powerd/system/udev.h"

using std::map;
using std::string;
using std::vector;

namespace power_manager {
namespace system {

namespace {

const char kSysClassInputPath[] = "/sys/class/input";
const char kDevInputPath[] = "/dev/input";
const char kInputBaseName[] = "event";

const char kInputMatchPattern[] = "input*";
const char kUsbMatchString[] = "usb";
const char kBluetoothMatchString[] = "bluetooth";

// Path to the console device where VT_GETSTATE ioctls are made to get the
// currently-active VT.
const char kConsolePath[] = "/dev/tty0";

// Physical location (as returned by EVIOCGPHYS()) of power button devices that
// should be skipped.
//
// Skip input events from the ACPI power button (identified as LNXPWRBN) if a
// new power button is present on the keyboard.
const char kPowerButtonToSkip[] = "LNXPWRBN";

// Skip input events that are on the built-in keyboard if a legacy power button
// is used. Many of these devices advertise a power button but do not physically
// have one. Skipping will reduce the wasteful waking of powerd due to keyboard
// events.
const char kPowerButtonToSkipForLegacy[] = "isa";

// Given a string |name| consisting of kInputBaseName followed by a base-10
// integer, extracts the integer to |num_out|. Returns false if |name| didn't
// match the expected format.
bool GetInputNumber(const std::string& name, int* num_out) {
  if (!StartsWithASCII(name, kInputBaseName, true))
    return false;
  size_t base_len = strlen(kInputBaseName);
  return base::StringToInt(name.substr(base_len, name.size() - base_len),
                           num_out);
}

// If |event| came from a lid switch, copies its state to |state_out| and
// returns true. Otherwise, leaves |state_out| untouched and returns false.
bool GetLidStateFromEvent(const input_event& event, LidState* state_out) {
  if (event.type != EV_SW || event.code != SW_LID)
    return false;

  *state_out = event.value == 1 ? LID_CLOSED : LID_OPEN;
  return true;
}

// If |event| came from a power button, copies its state to |state_out| and
// returns true. Otherwise, leaves |state_out| untouched and returns false.
bool GetPowerButtonStateFromEvent(const input_event& event,
                                  ButtonState* state_out) {
  if (event.type != EV_KEY || event.code != KEY_POWER)
    return false;

  switch (event.value) {
    case 0: *state_out = BUTTON_UP;      break;
    case 1: *state_out = BUTTON_DOWN;    break;
    case 2: *state_out = BUTTON_REPEAT;  break;
    default: LOG(ERROR) << "Unhandled button state " << event.value;
  }
  return true;
}

}  // namespace

const char InputWatcher::kInputUdevSubsystem[] = "input";

InputWatcher::InputWatcher()
    : lid_device_(NULL),
      use_lid_(true),
      lid_state_(LID_OPEN),
      power_button_to_skip_(kPowerButtonToSkip),
      console_fd_(-1),
      udev_(NULL) {
}

InputWatcher::~InputWatcher() {
  if (udev_)
    udev_->RemoveSubsystemObserver(kInputUdevSubsystem, this);
  if (console_fd_ >= 0)
    close(console_fd_);
}

bool InputWatcher::Init(
    scoped_ptr<EventDeviceFactoryInterface> event_device_factory,
    PrefsInterface* prefs,
    UdevInterface* udev) {
  event_device_factory_ = event_device_factory.Pass();
  udev_ = udev;

  prefs->GetBool(kUseLidPref, &use_lid_);
  if (!use_lid_)
    lid_state_ = LID_NOT_PRESENT;

  bool legacy_power_button = false;
  if (prefs->GetBool(kLegacyPowerButtonPref, &legacy_power_button) &&
      legacy_power_button)
    power_button_to_skip_ = kPowerButtonToSkipForLegacy;

  udev_->AddSubsystemObserver(kInputUdevSubsystem, this);

  // Don't bother doing anything more if we're running under a test.
  if (!sysfs_input_path_for_testing_.empty())
    return true;

  if ((console_fd_ = open(kConsolePath, O_WRONLY)) == -1)
    PLOG(ERROR) << "Unable to open " << kConsolePath;

  base::FilePath input_dir(kDevInputPath);
  if (!base::DirectoryExists(input_dir) ||
      access(kDevInputPath, R_OK|X_OK) != 0) {
    LOG(ERROR) << input_dir.value() << " isn't a readable directory";
    return false;
  }
  base::FileEnumerator enumerator(
      input_dir, false, base::FileEnumerator::FILES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    const std::string name = path.BaseName().value();
    int num = -1;
    if (GetInputNumber(name, &num))
      HandleAddedInput(name, num);
  }
  return true;
}

void InputWatcher::AddObserver(InputObserver* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void InputWatcher::RemoveObserver(InputObserver* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

LidState InputWatcher::QueryLidState() {
  if (!lid_device_)
    return LID_NOT_PRESENT;

  while (true) {
    // Stop when we fail to read any more events.
    std::vector<input_event> events;
    if (!lid_device_->ReadEvents(&events))
      break;

    // Get the state from the last lid event (|events| may also contain non-lid
    // events).
    for (std::vector<input_event>::const_reverse_iterator it =
             events.rbegin(); it != events.rend(); ++it) {
      if (GetLidStateFromEvent(*it, &lid_state_))
        break;
    }

    queued_events_.insert(queued_events_.end(), events.begin(), events.end());
    VLOG(1) << "Queued " << events.size()
            << " event(s) while querying lid state";
  }

  if (!queued_events_.empty()) {
    send_queued_events_task_.Reset(
        base::Bind(&InputWatcher::SendQueuedEvents, base::Unretained(this)));
    base::MessageLoop::current()->PostTask(
        FROM_HERE, send_queued_events_task_.callback());
  }

  return lid_state_;
}

bool InputWatcher::IsUSBInputDeviceConnected() const {
  base::FileEnumerator enumerator(
      sysfs_input_path_for_testing_.empty() ?
          base::FilePath(kSysClassInputPath) :
          sysfs_input_path_for_testing_,
      false,
      static_cast<::base::FileEnumerator::FileType>(
          base::FileEnumerator::FILES | base::FileEnumerator::SHOW_SYM_LINKS),
      kInputMatchPattern);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    base::FilePath symlink_path;
    if (!base::ReadSymbolicLink(path, &symlink_path))
      continue;
    const std::string& path_string = symlink_path.value();
    // Skip bluetooth devices, which may be identified as USB devices.
    if (path_string.find(kBluetoothMatchString) != std::string::npos)
      continue;
    // Now look for the USB devices that are not bluetooth.
    size_t position = path_string.find(kUsbMatchString);
    if (position == std::string::npos)
      continue;
    // Now that the string "usb" has been found, make sure it is a whole word
    // and not just part of another word like "busbreaker".
    bool usb_at_word_head =
        position == 0 || !IsAsciiAlpha(path_string.at(position - 1));
    bool usb_at_word_tail =
        position + strlen(kUsbMatchString) == path_string.size() ||
        !IsAsciiAlpha(path_string.at(position + strlen(kUsbMatchString)));
    if (usb_at_word_head && usb_at_word_tail)
      return true;
  }
  return false;
}

int InputWatcher::GetActiveVT() {
  struct vt_stat state;
  if (ioctl(console_fd_, VT_GETSTATE, &state) == -1) {
    PLOG(ERROR) << "VT_GETSTATE ioctl on " << kConsolePath << "failed";
    return -1;
  }
  return state.v_active;
}

void InputWatcher::OnUdevEvent(const std::string& subsystem,
                               const std::string& sysname,
                        UdevAction action) {
  DCHECK_EQ(subsystem, kInputUdevSubsystem);
  int input_num = -1;
  if (GetInputNumber(sysname, &input_num)) {
    if (action == UDEV_ACTION_ADD)
      HandleAddedInput(sysname, input_num);
    else if (action == UDEV_ACTION_REMOVE)
      HandleRemovedInput(input_num);
  }
}

void InputWatcher::OnNewEvents(EventDeviceInterface* device) {
  SendQueuedEvents();

  std::vector<input_event> events;
  if (!device->ReadEvents(&events))
    return;

  VLOG(1) << "Read " << events.size() << " event(s) from "
          << device->GetDebugName();
  for (size_t i = 0; i < events.size(); ++i) {
    if (device == lid_device_)
      GetLidStateFromEvent(events[i], &lid_state_);
    NotifyObserversAboutEvent(events[i]);
  }
}

void InputWatcher::HandleAddedInput(const std::string& input_name,
                                    int input_num) {
  if (event_devices_.count(input_num) > 0) {
    LOG(WARNING) << "Input " << input_num << " already registered";
    return;
  }

  base::FilePath path = base::FilePath(kDevInputPath).Append(input_name);
  linked_ptr<EventDeviceInterface> device(
      event_device_factory_->Open(path.value()));
  if (!device.get()) {
    LOG(ERROR) << "Failed to open " << path.value();
    return;
  }

  const std::string phys = device->GetPhysPath();
  if (StartsWithASCII(phys, power_button_to_skip_, true /* case_sensitive */)) {
    VLOG(1) << "Skipping event device with phys path: " << phys;
    return;
  }

  bool should_watch = false;
  if (device->IsPowerButton()) {
    LOG(INFO) << "Watching power button: " << device->GetDebugName();
    should_watch = true;
  }

  // Note that it's possible for a power button and lid switch to share a single
  // event device.
  if (use_lid_ && device->IsLidSwitch()) {
    LOG(INFO) << "Watching lid switch: " << device->GetDebugName();
    should_watch = true;

    if (lid_device_)
      LOG(WARNING) << "Multiple lid devices found on system";
    lid_device_ = device.get();

    lid_state_ = device->GetInitialLidState();
    VLOG(1) << "Initial lid state is " << LidStateToString(lid_state_);
  }

  if (should_watch) {
    device->WatchForEvents(base::Bind(&InputWatcher::OnNewEvents,
                                      base::Unretained(this),
                                      base::Unretained(device.get())));
    event_devices_.insert(std::make_pair(input_num, device));
  }
}


void InputWatcher::HandleRemovedInput(int input_num) {
  InputMap::iterator it = event_devices_.find(input_num);
  if (it == event_devices_.end())
    return;
  LOG(INFO) << "Stopping watching " << it->second->GetDebugName();
  if (lid_device_ == it->second.get())
    lid_device_ = NULL;
  event_devices_.erase(it);
}

void InputWatcher::SendQueuedEvents() {
  for (size_t i = 0; i < queued_events_.size(); ++i)
    NotifyObserversAboutEvent(queued_events_[i]);
  queued_events_.clear();
}

void InputWatcher::NotifyObserversAboutEvent(const input_event& event) {
  LidState lid_state = LID_OPEN;
  if (GetLidStateFromEvent(event, &lid_state)) {
    VLOG(1) << "Notifying observers about lid " << LidStateToString(lid_state)
            << " event";
    FOR_EACH_OBSERVER(InputObserver, observers_, OnLidEvent(lid_state));
  }

  ButtonState button_state = BUTTON_DOWN;
  if (GetPowerButtonStateFromEvent(event, &button_state)) {
    VLOG(1) << "Notifying observers about power button "
            << ButtonStateToString(button_state) << " event";
    FOR_EACH_OBSERVER(InputObserver, observers_,
                      OnPowerButtonEvent(button_state));
  }
}

}  // namespace system
}  // namespace power_manager
