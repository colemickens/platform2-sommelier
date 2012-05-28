// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/input.h"

#include <dirent.h>
#include <fcntl.h>
#include <glib.h>
#include <linux/input.h>

#include "base/file_util.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"

using std::map;
using std::string;
using std::vector;

namespace {

const char kInputUdevSubsystem[] = "input";
const char kSysClassInputPath[] = "/sys/class/input";
const char kDevInputPath[] = "/dev/input";
const char kEventBaseName[] = "event";
const char kInputBaseName[] = "input";

const char kWakeupDisabled[] = "disabled";
const char kWakeupEnabled[] = "enabled";

power_manager::InputType GetInputType(const struct input_event& event) {
  if (event.type == EV_KEY) {
    // For key events, only handle the keys listed below.
    switch (event.code) {
      case KEY_POWER:     return power_manager::INPUT_POWER_BUTTON;
      case KEY_F13:       return power_manager::INPUT_LOCK_BUTTON;
      case KEY_F4:        return power_manager::INPUT_KEY_F4;
      case KEY_LEFTCTRL:  return power_manager::INPUT_KEY_LEFT_CTRL;
      case KEY_RIGHTCTRL: return power_manager::INPUT_KEY_RIGHT_CTRL;
      case KEY_LEFTALT:   return power_manager::INPUT_KEY_LEFT_ALT;
      case KEY_RIGHTALT:  return power_manager::INPUT_KEY_RIGHT_ALT;
      case KEY_LEFTSHIFT: return power_manager::INPUT_KEY_LEFT_SHIFT;
      case KEY_RIGHTSHIFT:return power_manager::INPUT_KEY_RIGHT_SHIFT;
      default:            return power_manager::INPUT_UNHANDLED;
    }
  }
  // For switch events, only handle events from the lid.
  if (event.type == EV_SW && event.code == SW_LID)
    return power_manager::INPUT_LID;

  return power_manager::INPUT_UNHANDLED;
}

const char* InputTypeToString(power_manager::InputType type) {
  switch (type) {
    case power_manager::INPUT_LID:            return "input(LID)";
    case power_manager::INPUT_POWER_BUTTON:   return "input(POWER_BUTTON)";
    case power_manager::INPUT_LOCK_BUTTON:    return "input(LOCK_BUTTON)";
    case power_manager::INPUT_KEY_LEFT_CTRL:  return "input(KEY_LEFT_CTRL)";
    case power_manager::INPUT_KEY_RIGHT_CTRL: return "input(KEY_RIGHT_CTRL)";
    case power_manager::INPUT_KEY_LEFT_ALT:   return "input(KEY_LEFT_ALT)";
    case power_manager::INPUT_KEY_RIGHT_ALT:  return "input(KEY_RIGHT_ALT)";
    case power_manager::INPUT_KEY_LEFT_SHIFT: return "input(KEY_LEFT_SHIFT)";
    case power_manager::INPUT_KEY_RIGHT_SHIFT:return "input(KEY_RIGHT_SHIFT)";
    case power_manager::INPUT_KEY_F4:         return "input(KEY_F4)";
    case power_manager::INPUT_UNHANDLED:      return "input(UNHANDLED)";
    default:                                  NOTREACHED(); return "";
  }
}

bool GetSuffixNumber(const char* name, const char* base_name, int* suffix) {
  if (strncmp(base_name, name, strlen(base_name)))
    return false;
  return base::StringToInt(name + strlen(base_name), suffix);
}

}  // namespace

namespace power_manager {

Input::Input()
    : handler_(NULL),
      handler_data_(NULL),
      lid_fd_(-1),
      num_power_key_events_(0),
      num_lid_events_(0),
      wakeups_enabled_(true) {}

Input::~Input() {
  // TODO(bleung) : track and close ALL handles that we have open, and shutdown
  // g_io_channels as well.
  if (lid_fd_ >= 0)
    close(lid_fd_);
}

bool Input::Init(const vector<string>& wakeup_input_names) {
  for (vector<string>::const_iterator names_iter = wakeup_input_names.begin();
      names_iter != wakeup_input_names.end(); ++names_iter) {
    // Iterate through the vector of input names, and if not the empty string,
    // put the input name into the wakeup_inputs_map_, mapping to -1.
    // This indicates looking for input devices with this name, but there
    // is no input number associated with the device just yet.
    if ((*names_iter).length() > 0)
      wakeup_inputs_map_[*names_iter] = -1;
  }

  RegisterUdevEventHandler();
  RegisterInputWakeSources();
  return RegisterInputDevices();
}

#define BITS_PER_LONG (sizeof(long) * 8)
#define NUM_BITS(x) ((((x) - 1) / BITS_PER_LONG) + 1)
#define OFF(x)  ((x) % BITS_PER_LONG)
#define BIT(x)  (1UL << OFF(x))
#define LONG(x) ((x) / BITS_PER_LONG)
#define IS_BIT_SET(bit, array)  ((array[LONG(bit)] >> OFF(bit)) & 1)

bool Input::QueryLidState(int* lid_state) {
  if (0 > lid_fd_) {
    LOG(ERROR) << "No lid found on system.";
    return false;
  }
  unsigned long switch_events[NUM_BITS(SW_LID + 1)];
  memset(switch_events, 0, sizeof(switch_events));
  if (ioctl(lid_fd_, EVIOCGBIT(EV_SW, SW_LID + 1), switch_events) < 0) {
    LOG(ERROR) << "Error in GetLidState ioctl";
    return false;
  }
  if (IS_BIT_SET(SW_LID, switch_events)) {
    ioctl(lid_fd_, EVIOCGSW(sizeof(switch_events)), switch_events);
    *lid_state = IS_BIT_SET(SW_LID, switch_events);
    return true;
  } else {
    return false;
  }
}

bool Input::DisableWakeInputs() {
  wakeups_enabled_ = false;
  return SetInputWakeupStates();
}

bool Input::EnableWakeInputs() {
  wakeups_enabled_ = true;
  return SetInputWakeupStates();
}

bool Input::RegisterInputDevices() {
  FilePath input_path(kDevInputPath);
  DIR* dir = opendir(input_path.value().c_str());
  int num_registered = 0;
  if (dir) {
    struct dirent entry;
    struct dirent* result;
    while (readdir_r(dir, &entry, & result) == 0 && result) {
      if (result->d_name[0]) {
        if (AddEvent(result->d_name))
          num_registered++;
      }
    }
  } else {
    LOG(ERROR) << "Cannot open input dir : " << input_path.value().c_str();
    return false;
  }

  LOG(INFO) << "Number of power key events registered : "
            << num_power_key_events_;

  // Allow max of one lid.
  if (num_lid_events_ > 1) {
    LOG(ERROR) << "No lid events registered.";
    return false;
  }
  LOG(INFO) << "Number of lid events registered : " << num_lid_events_;
  return true;
}

bool Input::RegisterInputWakeSources() {
  DIR* dir = opendir(kSysClassInputPath);
  if (dir) {
    struct dirent entry;
    struct dirent* result;
    while (readdir_r(dir, &entry, &result) == 0 && result)
      if (result->d_name[0] &&
          strncmp(result->d_name, kInputBaseName, strlen(kInputBaseName)) == 0)
        AddWakeInput(result->d_name);
  }
  return true;
}

bool Input::SetInputWakeupStates() {
  bool result = true;
  for (WakeupMap::iterator iter = wakeup_inputs_map_.begin();
       iter != wakeup_inputs_map_.end(); iter++) {
    int input_num = (*iter).second;
    if (input_num != -1 && !SetWakeupState(input_num, wakeups_enabled_)) {
      result = false;
      LOG(WARNING) << "Failed to set power/wakeup for input" << input_num;
    }
  }
  return result;
}

bool Input::SetWakeupState(int input_num, bool enabled) {
  // Allocate a buffer of size enough to fit the basename "input"
  // with an integer up to maxint.
  // The number of digits required to hold a number k represented in base b
  // is ceil(logb(k)).
  // In this case, base b=10 and k = 256^n, where n=number of bytes.
  // So number of digits = ceil(log10(256^n)) = ceil (n log10(256))
  // = ceil(2.408 n) <= 3 * n.
  // + 1 for null terminator.
  char name[strlen(kInputBaseName) + sizeof(input_num) * 3 + 1];

  snprintf(name, sizeof(name), "%s%d", kInputBaseName, input_num);
  FilePath input_path = FilePath(kSysClassInputPath).Append(name);

  // wakeup sysfs is at /sys/class/input/inputX/device/power/wakeup
  FilePath wakeup_path = input_path.Append("device/power/wakeup");
  if (access(wakeup_path.value().c_str(), R_OK)) {
    LOG(WARNING) << "Failed to access power/wakeup for : " << name;
    return false;
  }

  const char* wakeup_str = enabled ? kWakeupEnabled : kWakeupDisabled;
  if (!file_util::WriteFile(wakeup_path, wakeup_str, strlen(wakeup_str))) {
    LOG(ERROR) << "Failed to write to power/wakeup.";
    return false;
  }

  LOG(INFO) << "Set power/wakeup for input" << input_num << " state: "
            << wakeup_str;
  return true;
}

bool Input::AddEvent(const char* name) {
  int event_num = -1;
  if (!GetSuffixNumber(name, kEventBaseName, &event_num))
    return false;

  InputMap::iterator iter = registered_inputs_.find(event_num);
  if (iter != registered_inputs_.end()) {
    LOG(WARNING) << "Input event " << event_num << " already registered.";
    return false;
  }

  FilePath event_path = FilePath(kDevInputPath).Append(name);
  int event_fd;
  if (access(event_path.value().c_str(), R_OK)) {
    LOG(WARNING) << "Failed to read from device.";
    return false;
  }
  if ((event_fd = open(event_path.value().c_str(), O_RDONLY)) < 0) {
    LOG(ERROR) << "Failed to open - " << event_path.value().c_str();
    return false;
  }

  guint tag;
  GIOChannel* channel = RegisterInputEvent(event_fd, &tag);
  if (!channel) {
    if (close(event_fd) < 0)  // event not registered, closing.
      LOG(ERROR) << "Error closing file handle.";
    return false;
  }
  IOChannelWatch desc;
  desc.channel = channel;
  desc.source_id = tag;
  // The tag should be valid if there was a successful event registration.
  // Thus, if the tag turns out to be valid, log a warning instead of failing
  // or skipping this part.
  LOG_IF(WARNING, tag == 0) << "Invalid glib source for event " << name;
  registered_inputs_[event_num] = desc;
  return true;
}

bool Input::RemoveEvent(const char* name) {
  int event_num = -1;
  if (!GetSuffixNumber(name, kEventBaseName, &event_num))
    return false;

  InputMap::iterator iter = registered_inputs_.find(event_num);
  if (iter == registered_inputs_.end()) {
    LOG(WARNING) << "Input event "
                 << event_num
                 << " not registered. Nothing to remove.";
    return false;
  }

  IOChannelWatch channel_descriptor = iter->second;
  guint tag = channel_descriptor.source_id;
  // The tag should not be invalid (see AddEvent()).  So log a warning instead
  // of failing or skipping.
  LOG_IF(WARNING, tag == 0) << "Attempting to remove invalid glib source.";
  if (g_source_remove(tag))
    DLOG(INFO) << "Watch removed successfully!";
  else
    DLOG(INFO) << "Remove of watch failed!";
  registered_inputs_.erase(iter);
  return true;
}

bool Input::AddWakeInput(const char* name) {
  int input_num = -1;
  if (wakeup_inputs_map_.empty() ||
      !GetSuffixNumber(name, kInputBaseName, &input_num))
    return false;

  FilePath device_name_path =
      FilePath(kSysClassInputPath).Append(name).Append("name");
  if (access(device_name_path.value().c_str(), R_OK)) {
    LOG(WARNING) << "Failed to access input name.";
    return false;
  }

  std::string input_name;
  if (!file_util::ReadFileToString(device_name_path, &input_name)) {
    LOG(WARNING) << "Failed to read input name.";
    return false;
  }
  TrimWhitespaceASCII(input_name, TRIM_TRAILING, &input_name);
  WakeupMap::iterator iter = wakeup_inputs_map_.find(input_name);
  if (iter == wakeup_inputs_map_.end()) {
    // Not on the list of wakeup input devices
    return false;
  }

  if (!SetWakeupState(input_num, wakeups_enabled_)) {
    LOG(ERROR) << "Error Adding Wakeup source. Cannot write to power/wakeup.";
    return false;
  }
  wakeup_inputs_map_[input_name] = input_num;
  return true;
}

bool Input::RemoveWakeInput(const char* name) {
  int input_num = -1;
  if (wakeup_inputs_map_.empty() ||
      !GetSuffixNumber(name, kInputBaseName, &input_num))
    return false;

  WakeupMap::iterator iter;
  for (iter = wakeup_inputs_map_.begin();
       iter != wakeup_inputs_map_.end(); iter++) {
    if ((*iter).second == input_num) {
      wakeup_inputs_map_[(*iter).first] = -1;
      LOG(INFO) << "Remove wakeup source " << (*iter).first << " was:input"
                << input_num;
    }
  }
  return false;
}

GIOChannel* Input::RegisterInputEvent(int fd, guint* tag) {
  char name[256] = "Unknown";
  if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) < 0) {
    LOG(ERROR) << "Could not get name of this device.";
    return NULL;
  } else {
    LOG(INFO) << "Device name : " << name;
  }

  char phys[256] = "Unknown";
  if (ioctl(fd, EVIOCGPHYS(sizeof(phys)), phys) < 0) {
    LOG(ERROR) << "Could not get topo phys path of this device.";
    return NULL;
  } else {
    LOG(INFO) << "Device topo phys : " << phys;
  }

#ifdef NEW_POWER_BUTTON
  // Skip input events from the ACPI power button (identified as LNXPWRBN) if
  // a new power button is present. In that case, don't skip the built in
  // keyboard, which starts with isa in its topo phys path.
  if (0 == strncmp("LNXPWRBN", phys, 8)) {
    LOG(INFO) << "Skipping interface : " << phys;
    return NULL;
}
#else
  // Skip input events that are on the built in keyboard.
  // Many of these devices advertise a power key but do not physically have one.
  // Skipping will reduce the wasteful waking of powerm due to keyboard events.
  if (0 == strncmp("isa", phys, 3)) {
    LOG(INFO) << "Skipping interface : " << phys;
    return NULL;
  }
#endif

  unsigned long events[NUM_BITS(EV_MAX)];
  memset(events, 0, sizeof(events));
  if (ioctl(fd, EVIOCGBIT(0, EV_MAX), events) < 0) {
    LOG(ERROR) << "Error in powerm ioctl - event list";
    return NULL;
  }

  GIOChannel* channel = NULL;
  bool watch_added = false;
  if (IS_BIT_SET(EV_KEY, events)) {
    unsigned long keys[NUM_BITS(KEY_MAX)];
    memset(keys, 0, sizeof(keys));
    if (ioctl(fd, EVIOCGBIT(EV_KEY, KEY_MAX), keys) < 0) {
      LOG(ERROR) << "Error in powerm ioctl - key";
    }
    if (IS_BIT_SET(KEY_POWER, keys) || IS_BIT_SET(KEY_F13, keys)) {
      LOG(INFO) << "Watching this event for power/lock buttons!";
      channel = g_io_channel_unix_new(fd);
      *tag = g_io_add_watch(channel, G_IO_IN, &(Input::EventHandler), this);
      num_power_key_events_++;
      watch_added = true;
    }
  }
  if (IS_BIT_SET(EV_SW, events)) {
    unsigned long switch_events[NUM_BITS(SW_LID + 1)];
    memset(switch_events, 0, sizeof(switch_events));
    if (ioctl(fd, EVIOCGBIT(EV_SW, SW_LID + 1), switch_events) < 0) {
      LOG(ERROR) << "Error in powerm ioctl - switch_events";
    }
    // An input event may have more than one kind of key or switch.
    // For example, if both the power button and the lid switch are handled
    // by the gpio_keys driver, both will share a single event in /dev/input.
    // In this case, only create one io channel per fd, and only add one
    // watch per event file.
    if (IS_BIT_SET(SW_LID, switch_events)) {
      num_lid_events_++;
      if (!watch_added) {
          LOG(INFO) << "Watching this event for lid switch!";
          channel = g_io_channel_unix_new(fd);
          *tag = g_io_add_watch(channel, G_IO_IN, &(Input::EventHandler), this);
          watch_added = true;
      } else {
        LOG(INFO) << "Watched event also has a lid!";
      }
      if (lid_fd_ >= 0)
        LOG(WARNING) << "Multiple lid events found on system!";
      lid_fd_ = fd;
    }
  }
  return channel;
}

gboolean Input::UdevEventHandler(GIOChannel* /* source */,
                                 GIOCondition /* condition */,
                                 gpointer data) {
  Input* input = static_cast<Input*>(data);

  struct udev_device* dev = udev_monitor_receive_device(input->udev_monitor_);
  if (dev) {
    const char* action = udev_device_get_action(dev);
    const char* sysname = udev_device_get_sysname(dev);
    LOG(INFO) << "Event on ("
              << udev_device_get_subsystem(dev)
              << ") Action "
              << udev_device_get_action(dev)
              << " sys name "
              << udev_device_get_sysname(dev);
    if (strncmp(kEventBaseName, sysname, strlen(kEventBaseName)) == 0) {
      if (strcmp("add", action) == 0)
        input->AddEvent(sysname);
      else if (strcmp("remove", action) == 0)
        input->RemoveEvent(sysname);
    } else if (strncmp(kInputBaseName, sysname, strlen(kInputBaseName)) == 0) {
      if (strcmp("add", action) == 0)
        input->AddWakeInput(sysname);
      else if (strcmp("remove", action) == 0)
        input->RemoveWakeInput(sysname);
    }
    udev_device_unref(dev);
  } else {
    LOG(ERROR) << "Can't get receive_device()";
    return false;
  }
  return true;
}

void Input::RegisterUdevEventHandler() {
  // Create the udev object.
  udev_ = udev_new();
  if (!udev_)
    LOG(ERROR) << "Can't create udev object.";

  // Create the udev monitor structure.
  udev_monitor_ = udev_monitor_new_from_netlink(udev_, "udev");
  if (!udev_monitor_) {
    LOG(ERROR) << "Can't create udev monitor.";
    udev_unref(udev_);
  }
  udev_monitor_filter_add_match_subsystem_devtype(udev_monitor_,
                                                  kInputUdevSubsystem,
                                                  NULL);
  udev_monitor_enable_receiving(udev_monitor_);

  int fd = udev_monitor_get_fd(udev_monitor_);

  GIOChannel* channel = g_io_channel_unix_new(fd);
  g_io_add_watch(channel, G_IO_IN, &(Input::UdevEventHandler), this);

  LOG(INFO) << "Udev controller waiting for events on subsystem "
            << kInputUdevSubsystem;
}

gboolean Input::EventHandler(GIOChannel* source, GIOCondition condition,
                             gpointer data) {
  Input* input = static_cast<Input*>(data);
  if (condition != G_IO_IN)
    return false;
  struct input_event events[64];
  gint fd = g_io_channel_unix_get_fd(source);
  unsigned int read_size = read(fd, events, sizeof(struct input_event) * 64);
  if (read_size < static_cast<int>(sizeof(struct input_event))) {
    LOG(ERROR) << "failed reading";
    return true;
  }
  if (!input->handler_)
    return true;
  for (unsigned int i = 0; i < read_size / sizeof(struct input_event); i++) {
    InputType input_type = GetInputType(events[i]);
    if (input_type == INPUT_UNHANDLED)
      continue;
    LOG(INFO) << "Handling event: " << InputTypeToString(input_type);
    (*input->handler_)(input->handler_data_, input_type, events[i].value);
    LOG(INFO) << "Input event handled: " << InputTypeToString(input_type);
  }
  return true;
}

void Input::RegisterHandler(InputHandler handler, void* data) {
  handler_ = handler;
  handler_data_ = data;
}

}  // namespace power_manager
