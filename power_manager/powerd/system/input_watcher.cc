// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/input_watcher.h"

#include <fcntl.h>
#include <linux/input.h>
#include <linux/vt.h>
#include <sys/ioctl.h>
#include <unistd.h>

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
#include "power_manager/powerd/system/input_observer.h"
#include "power_manager/powerd/system/udev.h"

#define BITS_PER_LONG (sizeof(long) * 8)  // NOLINT(runtime/int)
#define NUM_BITS(x) ((((x) - 1) / BITS_PER_LONG) + 1)
#define OFF(x)  ((x) % BITS_PER_LONG)
#define BIT(x)  (1UL << OFF(x))
#define LONG(x) ((x) / BITS_PER_LONG)
#define IS_BIT_SET(bit, array)  ((array[LONG(bit)] >> OFF(bit)) & 1)

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

// Takes ownership of a file descriptor and watches it for readability.
class InputWatcher::InputFileDescriptor {
 public:
  // Takes ownership of |fd| and closes it on destruction. |watcher| is notified
  // when |fd| becomes readable.
  InputFileDescriptor(int fd, base::MessageLoopForIO::Watcher* watcher)
      : fd_(fd),
        fd_watcher_(new base::MessageLoopForIO::FileDescriptorWatcher) {
    if (!base::MessageLoopForIO::current()->WatchFileDescriptor(
            fd, true, base::MessageLoopForIO::WATCH_READ, fd_watcher_.get(),
            watcher)) {
      LOG(ERROR) << "Unable to watch FD " << fd;
    }
  }

  ~InputFileDescriptor() {
    fd_watcher_.reset();
    // ENODEV is expected if the device was just unplugged.
    if (close(fd_) != 0 && errno != ENODEV)
      PLOG(ERROR) << "Unable to close FD " << fd_;
  }

  int fd() const { return fd_; }

 private:
  // File descriptor that is being watched.
  int fd_;

  // Controller used to watch |fd_|.
  scoped_ptr<base::MessageLoopForIO::FileDescriptorWatcher> fd_watcher_;

  DISALLOW_COPY_AND_ASSIGN(InputFileDescriptor);
};

InputWatcher::InputWatcher()
    : lid_fd_(-1),
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

bool InputWatcher::Init(PrefsInterface* prefs, UdevInterface* udev) {
  prefs->GetBool(kUseLidPref, &use_lid_);
  if (!use_lid_)
    lid_state_ = LID_NOT_PRESENT;

  bool legacy_power_button = false;
  if (prefs->GetBool(kLegacyPowerButtonPref, &legacy_power_button) &&
      legacy_power_button)
    power_button_to_skip_ = kPowerButtonToSkipForLegacy;

  udev_ = udev;
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
  if (lid_fd_ < 0)
    return LID_NOT_PRESENT;

  while (true) {
    // Stop when we fail to read any more events.
    std::vector<input_event> events;
    if (!ReadEvents(lid_fd_, &events))
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

void InputWatcher::OnFileCanReadWithoutBlocking(int fd) {
  SendQueuedEvents();

  std::vector<input_event> events;
  if (!ReadEvents(fd, &events))
    return;

  VLOG(1) << "Read " << events.size() << " event(s) from FD " << fd;
  for (size_t i = 0; i < events.size(); ++i) {
    GetLidStateFromEvent(events[i], &lid_state_);
    NotifyObserversAboutEvent(events[i]);
  }
}

void InputWatcher::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED() << "Unexpected non-blocking write notification for FD " << fd;
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

void InputWatcher::HandleAddedInput(const std::string& input_name,
                                    int input_num) {
  if (registered_inputs_.count(input_num) > 0) {
    LOG(WARNING) << "Input " << input_num << " already registered";
    return;
  }

  base::FilePath path = base::FilePath(kDevInputPath).Append(input_name);
  if (access(path.value().c_str(), R_OK) != 0) {
    LOG(WARNING) << "Missing read access to " << path.value();
    return;
  }

  int fd = -1;
  if ((fd = open(path.value().c_str(), O_RDONLY|O_NONBLOCK)) < 0) {
    PLOG(ERROR) << "open() failed for " << path.value();
    return;
  }

  // Wrap the FD in an InputFileDescriptor so it'll be automatically closed if
  // we return early.
  linked_ptr<InputFileDescriptor> linked_fd(new InputFileDescriptor(fd, this));

  char device_name[256] = "Unknown";
  if (ioctl(fd, EVIOCGNAME(sizeof(device_name)), device_name) < 0) {
    PLOG(ERROR) << "Could not get name of device (FD " << fd << ", input "
                << input_num << ")";
    return;
  }
  VLOG(1) << "Device name: " << device_name;

  char phys[256] = "Unknown";
  if (ioctl(fd, EVIOCGPHYS(sizeof(phys)), phys) < 0 && errno != ENOENT) {
    PLOG(ERROR) << "Could not get topo phys path of device " << device_name;
    return;
  }
  VLOG(1) << "Device topo phys: " << phys;

  if (StartsWithASCII(phys, power_button_to_skip_, true /* case_sensitive */)) {
    VLOG(1) << "Skipping interface: " << phys;
    return;
  }

  unsigned long events[NUM_BITS(EV_MAX)];  // NOLINT(runtime/int)
  memset(events, 0, sizeof(events));
  if (ioctl(fd, EVIOCGBIT(0, EV_MAX), events) < 0) {
    PLOG(ERROR) << "EV_MAX ioctl failed for device " << device_name;
    return;
  }

  bool should_watch = false;

  // Power button.
  if (IS_BIT_SET(EV_KEY, events)) {
    unsigned long keys[NUM_BITS(KEY_MAX)];  // NOLINT(runtime/int)
    memset(keys, 0, sizeof(keys));
    if (ioctl(fd, EVIOCGBIT(EV_KEY, KEY_MAX), keys) < 0) {
      PLOG(ERROR) << "KEY_MAX ioctl failed for device " << device_name;
    } else if (IS_BIT_SET(KEY_POWER, keys)) {
      LOG(INFO) << "Watching " << phys << " (" << device_name
                << ") for power button on FD " << fd;
      should_watch = true;
    }
  }

  // Lid switch. Note that it's possible for a power button and lid switch to
  // share a single event file.
  if (IS_BIT_SET(EV_SW, events)) {
    unsigned long switch_events[NUM_BITS(SW_LID + 1)];  // NOLINT(runtime/int)
    memset(switch_events, 0, sizeof(switch_events));
    if (ioctl(fd, EVIOCGBIT(EV_SW, SW_LID + 1), switch_events) < 0) {
      PLOG(ERROR) << "SW_LID ioctl failed for device " << device_name;
    } else if (use_lid_ && IS_BIT_SET(SW_LID, switch_events)) {
      LOG(INFO) << "Watching " << phys << " (" << device_name
                << ") for lid switch on FD " << fd;
      should_watch = true;

      if (lid_fd_ >= 0)
        LOG(WARNING) << "Multiple lid events found on system";
      lid_fd_ = fd;

      if (ioctl(lid_fd_, EVIOCGSW(sizeof(switch_events)), switch_events) < 0) {
        PLOG(ERROR) << "Reading initial lid state failed";
      } else {
        lid_state_ = IS_BIT_SET(SW_LID, switch_events) ? LID_CLOSED : LID_OPEN;
        VLOG(1) << "Initial lid state is " << LidStateToString(lid_state_);
      }
    }
  }

  if (should_watch)
    registered_inputs_.insert(std::make_pair(input_num, linked_fd));
}


void InputWatcher::HandleRemovedInput(int input_num) {
  InputMap::iterator iter = registered_inputs_.find(input_num);
  if (iter != registered_inputs_.end()) {
    LOG(INFO) << "Stopping watching FD " << iter->second->fd();
    if (iter->second->fd() == lid_fd_)
      lid_fd_ = -1;
    registered_inputs_.erase(iter);
  }
}

bool InputWatcher::ReadEvents(int fd, std::vector<input_event>* events_out) {
  DCHECK(events_out);
  events_out->clear();

  struct input_event events[64];
  ssize_t read_size = HANDLE_EINTR(read(fd, events, sizeof(events)));
  if (read_size < 0) {
    // ENODEV is expected if the device was just unplugged.
    if (errno != EAGAIN && errno != EWOULDBLOCK && errno != ENODEV)
      PLOG(ERROR) << "Reading events from FD " << fd << " failed";
    return false;
  } else if (read_size == 0) {
    LOG(ERROR) << "Didn't get any data when reading events from FD " << fd;
    return false;
  }

  const size_t num_events = read_size / sizeof(struct input_event);
  if (read_size % sizeof(struct input_event)) {
    LOG(ERROR) << "Read " << read_size << " byte(s) while expecting "
               << sizeof(struct input_event) << "-byte events";
    return false;
  }

  events_out->reserve(num_events);
  for (size_t i = 0; i < num_events; ++i)
    events_out->push_back(events[i]);
  return true;
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
