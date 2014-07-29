// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/input.h"

#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/vt.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <base/file_util.h>
#include <base/files/file_enumerator.h>
#include <base/files/file_path.h>
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
const char kEventBaseName[] = "event";
const char kInputBaseName[] = "input";

const char kWakeupDisabled[] = "disabled";
const char kWakeupEnabled[] = "enabled";

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

// Given a string |name| consisting of |base_name| followed by a base-10
// integer, extracts the integer to |suffix|. Returns false if |name| didn't
// match the expected format.
bool GetSuffixNumber(const std::string& name,
                     const std::string& base_name,
                     int* suffix) {
  size_t base_len = base_name.size();
  if (name.substr(0, base_len) != base_name)
    return false;
  return base::StringToInt(name.substr(base_len, name.size() - base_len),
                           suffix);
}

}  // namespace

const char Input::kInputUdevSubsystem[] = "input";

// Takes ownership of a file descriptor and watches it for readability.
class Input::EventFileDescriptor {
 public:
  // Takes ownership of |fd| and closes it on destruction. |watcher| is notified
  // when |fd| becomes readable.
  EventFileDescriptor(int fd, base::MessageLoopForIO::Watcher* watcher)
      : fd_(fd),
        fd_watcher_(new base::MessageLoopForIO::FileDescriptorWatcher) {
    if (!base::MessageLoopForIO::current()->WatchFileDescriptor(
            fd, true, base::MessageLoopForIO::WATCH_READ, fd_watcher_.get(),
            watcher)) {
      LOG(ERROR) << "Unable to watch FD " << fd;
    }
  }

  ~EventFileDescriptor() {
    fd_watcher_.reset();
    if (!close(fd_))
      PLOG(ERROR) << "Unable to close FD " << fd_;
  }

  int fd() const { return fd_; }

 private:
  // File descriptor that is being watched.
  int fd_;

  // Controller used to watch |fd_|.
  scoped_ptr<base::MessageLoopForIO::FileDescriptorWatcher> fd_watcher_;

  DISALLOW_COPY_AND_ASSIGN(EventFileDescriptor);
};

Input::Input()
    : lid_fd_(-1),
      num_power_key_events_(0),
      num_lid_events_(0),
      wakeups_enabled_(true),
      use_lid_(true),
      power_button_to_skip_(kPowerButtonToSkip),
      console_fd_(-1),
      udev_(NULL) {
}

Input::~Input() {
  if (udev_)
    udev_->RemoveObserver(kInputUdevSubsystem, this);
  if (console_fd_ >= 0)
    close(console_fd_);
}

bool Input::Init(PrefsInterface* prefs, UdevInterface* udev) {
  prefs->GetBool(kUseLidPref, &use_lid_);

  std::string wakeup_inputs_str;
  std::vector<std::string> wakeup_input_names;
  if (prefs->GetString(kWakeupInputPref, &wakeup_inputs_str))
    base::SplitString(wakeup_inputs_str, '\n', &wakeup_input_names);
  for (vector<string>::const_iterator names_iter = wakeup_input_names.begin();
      names_iter != wakeup_input_names.end(); ++names_iter) {
    // Iterate through the vector of input names, and if not the empty string,
    // put the input name into the wakeup_inputs_map_, mapping to -1.
    // This indicates looking for input devices with this name, but there
    // is no input number associated with the device just yet.
    if ((*names_iter).length() > 0)
      wakeup_inputs_map_[*names_iter] = -1;
  }

  bool legacy_power_button = false;
  if (prefs->GetBool(kLegacyPowerButtonPref, &legacy_power_button) &&
      legacy_power_button)
    power_button_to_skip_ = kPowerButtonToSkipForLegacy;

  udev_ = udev;
  udev_->AddObserver(kInputUdevSubsystem, this);

  // Don't bother doing anything more if we're running under a test.
  if (!sysfs_input_path_for_testing_.empty())
    return true;

  if ((console_fd_ = open(kConsolePath, O_WRONLY)) == -1)
    PLOG(ERROR) << "Unable to open " << kConsolePath;

  RegisterInputWakeSources();
  return RegisterInputDevices();
}

void Input::AddObserver(InputObserver* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void Input::RemoveObserver(InputObserver* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

#define BITS_PER_LONG (sizeof(long) * 8)  // NOLINT(runtime/int)
#define NUM_BITS(x) ((((x) - 1) / BITS_PER_LONG) + 1)
#define OFF(x)  ((x) % BITS_PER_LONG)
#define BIT(x)  (1UL << OFF(x))
#define LONG(x) ((x) / BITS_PER_LONG)
#define IS_BIT_SET(bit, array)  ((array[LONG(bit)] >> OFF(bit)) & 1)

LidState Input::QueryLidState() {
  if (lid_fd_ < 0)
    return LID_NOT_PRESENT;

  unsigned long switch_events[NUM_BITS(SW_LID + 1)];  // NOLINT(runtime/int)
  memset(switch_events, 0, sizeof(switch_events));
  if (ioctl(lid_fd_, EVIOCGBIT(EV_SW, SW_LID + 1), switch_events) < 0) {
    PLOG(ERROR) << "Lid state ioctl() failed";
    return LID_NOT_PRESENT;
  }
  if (IS_BIT_SET(SW_LID, switch_events)) {
    ioctl(lid_fd_, EVIOCGSW(sizeof(switch_events)), switch_events);
    return IS_BIT_SET(SW_LID, switch_events) ? LID_CLOSED : LID_OPEN;
  } else {
    return LID_NOT_PRESENT;
  }
}

bool Input::IsUSBInputDeviceConnected() const {
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

int Input::GetActiveVT() {
  struct vt_stat state;
  if (ioctl(console_fd_, VT_GETSTATE, &state) == -1) {
    PLOG(ERROR) << "VT_GETSTATE ioctl on " << kConsolePath << "failed";
    return -1;
  }
  return state.v_active;
}

void Input::SetInputDevicesCanWake(bool enable) {
  wakeups_enabled_ = enable;
  UpdateSysfsWakeup();
  UpdateAcpiWakeup();
}

void Input::OnFileCanReadWithoutBlocking(int fd) {
  struct input_event events[64];
  ssize_t read_size = HANDLE_EINTR(read(fd, events, sizeof(events)));
  if (read_size < 0)
    return;

  const ssize_t num_events = read_size / sizeof(struct input_event);
  if (!read_size || read_size % sizeof(struct input_event)) {
    LOG(ERROR) << "Read " << read_size << " byte(s) while expecting "
               << sizeof(struct input_event) << "-byte events";
    return;
  }

  for (ssize_t i = 0; i < num_events; i++) {
    const input_event& event = events[i];
    if (event.type == EV_SW && event.code == SW_LID) {
      LidState state = event.value == 1 ? LID_CLOSED : LID_OPEN;
      FOR_EACH_OBSERVER(InputObserver, observers_, OnLidEvent(state));
    } else if (event.type == EV_KEY && event.code == KEY_POWER) {
      ButtonState state = BUTTON_DOWN;
      switch (event.value) {
        case 0: state = BUTTON_UP;      break;
        case 1: state = BUTTON_DOWN;    break;
        case 2: state = BUTTON_REPEAT;  break;
        default: LOG(ERROR) << "Unhandled button state " << event.value;
      }
      FOR_EACH_OBSERVER(InputObserver, observers_, OnPowerButtonEvent(state));
    }
  }
}

void Input::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED() << "Unexpected non-blocking write notification for FD " << fd;
}

void Input::OnUdevEvent(const std::string& subsystem,
                        const std::string& sysname,
                        UdevObserver::Action action) {
  DCHECK_EQ(subsystem, kInputUdevSubsystem);
  if (StartsWithASCII(sysname, kEventBaseName, true)) {
    if (action == UdevObserver::ACTION_ADD)
      AddEvent(sysname);
    else if (action == UdevObserver::ACTION_REMOVE)
      RemoveEvent(sysname);
  } else if (StartsWithASCII(sysname, kInputBaseName, true)) {
    if (action == UdevObserver::ACTION_ADD)
      AddWakeInput(sysname);
    else if (action == UdevObserver::ACTION_REMOVE)
      RemoveWakeInput(sysname);
  }
}

bool Input::RegisterInputDevices() {
  DIR* dir = opendir(kDevInputPath);
  int num_registered = 0;
  if (!dir) {
    PLOG(ERROR) << "opendir() failed for " << kDevInputPath;
    return false;
  }

  struct dirent entry;
  struct dirent* result;
  while (readdir_r(dir, &entry, & result) == 0 && result) {
    if (result->d_name[0]) {
      if (AddEvent(result->d_name))
        num_registered++;
    }
  }
  if (closedir(dir) < 0)
    PLOG(ERROR) << "closedir() failed for " << kDevInputPath;

  LOG(INFO) << "Number of power button events registered: "
            << num_power_key_events_;
  LOG(INFO) << "Number of lid events registered: " << num_lid_events_;
  return true;
}

bool Input::RegisterInputWakeSources() {
  DIR* dir = opendir(kSysClassInputPath);
  if (!dir) {
    PLOG(ERROR) << "opendir() failed for " << kSysClassInputPath;
    return false;
  }

  struct dirent entry;
  struct dirent* result;
  while (readdir_r(dir, &entry, &result) == 0 && result) {
    if (StartsWithASCII(result->d_name, kInputBaseName, true))
      AddWakeInput(result->d_name);
  }
  if (closedir(dir) < 0)
    PLOG(ERROR) << "closedir() failed for " << kSysClassInputPath;
  return true;
}

bool Input::SetSysfsWakeup(int input_num, bool enabled) {
  std::string name = base::StringPrintf("%s%d", kInputBaseName, input_num);
  base::FilePath path = base::FilePath(kSysClassInputPath).
      Append(name).Append("device/power/wakeup");
  const char* state = enabled ? kWakeupEnabled : kWakeupDisabled;
  if (!util::WriteFileFully(path, state, strlen(state))) {
    PLOG(ERROR) << "Failed to write to " << path.value();
    return false;
  }
  LOG(INFO) << "Set " << path.value() << " to " << state;
  return true;
}

bool Input::UpdateSysfsWakeup() {
  bool result = true;
  for (WakeupMap::iterator iter = wakeup_inputs_map_.begin();
       iter != wakeup_inputs_map_.end(); iter++) {
    int input_num = iter->second;
    if (input_num != -1 && !SetSysfsWakeup(input_num, wakeups_enabled_)) {
      result = false;
      LOG(WARNING) << "Failed to set power/wakeup for input" << input_num;
    }
  }
  return result;
}

bool Input::UpdateAcpiWakeup() {
  // On x86 systems, setting power/wakeup in sysfs is not enough.
  // We disable touchscreen wakeup permanently, and we disable touchpad wakeup
  // whenever the lid is closed.

  AcpiWakeupHelper acpi_wakeup;
  if (!acpi_wakeup.IsSupported())
    return true;
  acpi_wakeup.SetWakeupEnabled("TSCR", false);
  acpi_wakeup.SetWakeupEnabled("TPAD", wakeups_enabled_);
  return true;
}

bool Input::AddEvent(const std::string& name) {
  // Avoid logging warnings for files that should be ignored.
  const char* kEventsToSkip[] = {
    ".",
    "..",
    "by-id",
    "by-path",
  };
  for (size_t i = 0; i < arraysize(kEventsToSkip); ++i) {
    if (name == kEventsToSkip[i])
      return false;
  }

  int event_num = -1;
  if (!GetSuffixNumber(name, kEventBaseName, &event_num)) {
    LOG(WARNING) << name << " is not a valid event name; not adding as event";
    return false;
  }

  InputMap::iterator iter = registered_inputs_.find(event_num);
  if (iter != registered_inputs_.end()) {
    LOG(WARNING) << "Input event " << event_num << " already registered";
    return false;
  }

  base::FilePath event_path = base::FilePath(kDevInputPath).Append(name);
  int event_fd;
  if (access(event_path.value().c_str(), R_OK) != 0) {
    LOG(WARNING) << "Missing read access to " << event_path.value();
    return false;
  }
  if ((event_fd = open(event_path.value().c_str(), O_RDONLY)) < 0) {
    PLOG(ERROR) << "open() failed for " << event_path.value();
    return false;
  }

  if (!RegisterInputEvent(event_fd, event_num)) {
    if (close(event_fd) < 0)  // event not registered, closing.
      PLOG(ERROR) << "close() failed for " << event_path.value();
    return false;
  }
  return true;
}

bool Input::RemoveEvent(const std::string& name) {
  int event_num = -1;
  if (!GetSuffixNumber(name, kEventBaseName, &event_num)) {
    LOG(WARNING) << name << " is not a valid event name; not removing event";
    return false;
  }

  InputMap::iterator iter = registered_inputs_.find(event_num);
  if (iter == registered_inputs_.end()) {
    LOG(WARNING) << "Input event " << name << " not registered; "
                 << "nothing to remove";
    return false;
  }

  registered_inputs_.erase(iter);
  return true;
}

bool Input::AddWakeInput(const std::string& name) {
  int input_num = -1;
  if (wakeup_inputs_map_.empty() ||
      !GetSuffixNumber(name, kInputBaseName, &input_num))
    return false;

  base::FilePath device_name_path =
      base::FilePath(kSysClassInputPath).Append(name).Append("name");
  if (access(device_name_path.value().c_str(), R_OK)) {
    LOG(WARNING) << "Missing read access to " << device_name_path.value();
    return false;
  }

  std::string input_name;
  if (!base::ReadFileToString(device_name_path, &input_name)) {
    LOG(WARNING) << "Failed to read input name from "
                 << device_name_path.value();
    return false;
  }
  base::TrimWhitespaceASCII(input_name, base::TRIM_TRAILING, &input_name);
  WakeupMap::iterator iter = wakeup_inputs_map_.find(input_name);
  if (iter == wakeup_inputs_map_.end()) {
    // Not on the list of wakeup input devices
    return false;
  }

  if (!SetSysfsWakeup(input_num, wakeups_enabled_)) {
    LOG(ERROR) << "Error adding wakeup source; cannot write to power/wakeup";
    return false;
  }
  wakeup_inputs_map_[input_name] = input_num;
  LOG(INFO) << "Added wakeup source " << name << " (" << input_name << ")";
  return true;
}

bool Input::RemoveWakeInput(const std::string& name) {
  int input_num = -1;
  if (wakeup_inputs_map_.empty() ||
      !GetSuffixNumber(name, kInputBaseName, &input_num))
    return false;

  WakeupMap::iterator iter;
  for (iter = wakeup_inputs_map_.begin();
       iter != wakeup_inputs_map_.end(); iter++) {
    if ((*iter).second == input_num) {
      wakeup_inputs_map_[(*iter).first] = -1;
      LOG(INFO) << "Removed wakeup source " << name
                << " (" << (*iter).first << ")";
    }
  }
  return false;
}

bool Input::RegisterInputEvent(int fd, int event_num) {
  char name[256] = "Unknown";
  if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) < 0) {
    PLOG(ERROR) << "Could not get name of device (FD " << fd << ", event "
                << event_num << ")";
    return false;
  } else {
    VLOG(1) << "Device name: " << name;
  }

  char phys[256] = "Unknown";
  if (ioctl(fd, EVIOCGPHYS(sizeof(phys)), phys) < 0 && errno != ENOENT) {
    PLOG(ERROR) << "Could not get topo phys path of device " << name;
    return false;
  } else {
    VLOG(1) << "Device topo phys: " << phys;
  }

  if (StartsWithASCII(phys, power_button_to_skip_, true /* case_sensitive */)) {
    VLOG(1) << "Skipping interface: " << phys;
    return false;
  }

  unsigned long events[NUM_BITS(EV_MAX)];  // NOLINT(runtime/int)
  memset(events, 0, sizeof(events));
  if (ioctl(fd, EVIOCGBIT(0, EV_MAX), events) < 0) {
    PLOG(ERROR) << "EV_MAX ioctl failed for device " << name;
    return false;
  }

  bool should_watch = false;

  // Power button.
  if (IS_BIT_SET(EV_KEY, events)) {
    unsigned long keys[NUM_BITS(KEY_MAX)];  // NOLINT(runtime/int)
    memset(keys, 0, sizeof(keys));
    if (ioctl(fd, EVIOCGBIT(EV_KEY, KEY_MAX), keys) < 0) {
      PLOG(ERROR) << "KEY_MAX ioctl failed for device " << name;
    } else if (IS_BIT_SET(KEY_POWER, keys)) {
      LOG(INFO) << "Watching " << phys << " (" << name << ") for power button";
      should_watch = true;
      num_power_key_events_++;
    }
  }

  // Lid switch. Note that it's possible for a power button and lid switch to
  // share a single event file.
  if (IS_BIT_SET(EV_SW, events)) {
    unsigned long switch_events[NUM_BITS(SW_LID + 1)];  // NOLINT(runtime/int)
    memset(switch_events, 0, sizeof(switch_events));
    if (ioctl(fd, EVIOCGBIT(EV_SW, SW_LID + 1), switch_events) < 0) {
      PLOG(ERROR) << "SW_LID ioctl failed for device " << name;
    } else if (use_lid_ && IS_BIT_SET(SW_LID, switch_events)) {
      LOG(INFO) << "Watching " << phys << " (" << name << ") for lid switch";
      should_watch = true;
      num_lid_events_++;

      if (lid_fd_ >= 0)
        LOG(WARNING) << "Multiple lid events found on system";
      lid_fd_ = fd;
    }
  }

  if (!should_watch)
    return false;

  registered_inputs_.insert(std::make_pair(event_num,
      make_linked_ptr(new EventFileDescriptor(fd, this))));
  return true;
}

}  // namespace system
}  // namespace power_manager
