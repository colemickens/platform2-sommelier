// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/input.h"

#include <dirent.h>
#include <fcntl.h>
#include <gdk/gdk.h>
#include <linux/input.h>
#include <stdlib.h>

#include "base/file_util.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/string_util.h"

using std::map;
using std::string;
using std::vector;

namespace power_manager {

const char kInputUdevSubsystem[] = "input";
const FilePath sys_class_input_path("/sys/class/input");
const char kEventBasename[] = "event";
const char kInputBasename[] = "input";

const char kWakeupDisabled[] = "disabled";
const char kWakeupEnabled[] = "enabled";

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
  for(vector<string>::const_iterator names_iter = wakeup_input_names.begin();
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
#define NBITS(x) ((((x)-1)/BITS_PER_LONG)+1)
#define OFF(x)  ((x)%BITS_PER_LONG)
#define BIT(x)  (1UL<<OFF(x))
#define LONG(x) ((x)/BITS_PER_LONG)
#define IS_BIT_SET(bit, array)  ((array[LONG(bit)] >> OFF(bit)) & 1)

bool Input::QueryLidState(int* lid_state) {
  unsigned long sw[NBITS(SW_LID + 1)];
  if (0 > lid_fd_) {
    LOG(ERROR) << "No lid found on system.";
    return false;
  }
  memset(sw, 0, sizeof(sw));
  if (ioctl(lid_fd_, EVIOCGBIT(EV_SW, SW_LID + 1), sw) < 0) {
    LOG(ERROR) << "Error in GetLidState ioctl";
    return false;
  }
  if (IS_BIT_SET(SW_LID, sw)) {
    ioctl(lid_fd_, EVIOCGSW(sizeof(sw)), sw);
    *lid_state = IS_BIT_SET(SW_LID, sw);
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
  FilePath input_path("/dev/input");
  DIR* dir = opendir(input_path.value().c_str());
  int num_registered = 0;
  bool retval = true;
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
  if (!num_power_key_events_) {
    LOG(ERROR) << "No power keys registered.";
    retval = false;
  } else {
    LOG(INFO) << "Number of power key events registered : "
              << num_power_key_events_;
  }
  // Allow max of one lid.
  if (num_lid_events_ > 1) {
    LOG(ERROR) << "No lid events registered.";
    retval = false;
  } else {
    LOG(INFO) << "Number of lid events registered : " << num_lid_events_;
  }
  return retval;
}

bool Input::RegisterInputWakeSources() {
  DIR* dir = opendir(sys_class_input_path.value().c_str());
  if (dir) {
    struct dirent entry;
    struct dirent* result;
    while (readdir_r(dir, &entry, &result) == 0 && result)
      if (result->d_name[0] &&
          strncmp(result->d_name, kInputBasename, strlen(kInputBasename)) == 0)
        AddWakeInput(result->d_name);
  }
  return true;
}

bool Input::SetInputWakeupStates() {
  bool ret = true;
  for (WakeupMap::iterator iter = wakeup_inputs_map_.begin();
       iter != wakeup_inputs_map_.end(); iter++) {
    int input_num = (*iter).second;
    if (input_num != -1 && !SetWakeupState(input_num, wakeups_enabled_)) {
      ret = false;
      LOG(WARNING) << "Failed to set power/wakeup for input" << input_num;
    }
  }
  return ret;
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
  char name[strlen(kInputBasename) + sizeof(input_num) * 3 + 1];

  sprintf(name, "%s%d", kInputBasename, input_num);
  FilePath input_path = sys_class_input_path.Append(name);

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

bool Input::AddEvent(const char * name) {
  int event_num = -1;
  if (strncmp(kEventBasename, name, strlen(kEventBasename)))
    return false;

  event_num = atoi(name + strlen(kEventBasename));

  InputMap::iterator iter = registered_inputs_.find(event_num);
  if (iter != registered_inputs_.end()) {
    LOG(WARNING) << "Input event " << event_num << " already registered.";
    return false;
  }

  FilePath input_path("/dev/input");
  FilePath event_path = input_path.Append(name);
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
    if (close(event_fd) < 0) // event not registered, closing.
      LOG(ERROR) << "Error closing file handle.";
    return false;
  }
  IOChannelWatch desc;
  desc.channel = channel;
  desc.sourcetag = tag;
  registered_inputs_[event_num] = desc;
  return true;
}

bool Input::RemoveEvent(const char* name) {
  int event_num = -1;
  if (strncmp(kEventBasename, name, strlen(kEventBasename)))
    return false;

  event_num = atoi(name + strlen(kEventBasename));
  InputMap::iterator iter = registered_inputs_.find(event_num);
  if (iter == registered_inputs_.end() ) {
    LOG(WARNING) << "Input event "
                 << event_num
                 << " not registered. Nothing to remove.";
    return false;
  }

  IOChannelWatch channel_descriptor = iter->second;
  guint tag = channel_descriptor.sourcetag;
  gboolean ret = g_source_remove(tag);
  if (!ret)
    DLOG(INFO) << "Remove of watch failed!";
  else
    DLOG(INFO) << "Watch removed successfully!";
  registered_inputs_.erase(iter);
  return true;
}

bool Input::AddWakeInput(const char* name) {
  if (strncmp(kInputBasename, name, strlen(kInputBasename)) ||
      wakeup_inputs_map_.empty())
    return false;

  FilePath input_path = sys_class_input_path.Append(name);
  FilePath device_name_path = input_path.Append("name");
  std::string input_name;

  if (access(device_name_path.value().c_str(), R_OK)) {
    LOG(WARNING) << "Failed to access input name.";
    return false;
  }
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

  int input_num = atoi(name + strlen(kInputBasename));
  if (!SetWakeupState(input_num, wakeups_enabled_)) {
    LOG(ERROR) << "Error Adding Wakeup source. Cannot write to power/wakeup.";
    return false;
  }
  wakeup_inputs_map_[input_name] = input_num;
  return true;
}

bool Input::RemoveWakeInput(const char* name) {
  if (strncmp(kInputBasename, name, strlen(kInputBasename)) ||
      wakeup_inputs_map_.empty())
    return false;

  int input_num = atoi(name + strlen(kInputBasename));
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
  unsigned long events[NBITS(EV_MAX)];
  char name[256] = "Unknown";
  char phys[256] = "Unknown";
  bool watch_added = false;

  if (ioctl(fd, EVIOCGNAME(sizeof(name)),name) < 0) {
    LOG(ERROR) << "Could not get name of this device.";
    return NULL;
  } else {
    LOG(INFO) << "Device name : " << name;
  }

  if (ioctl(fd, EVIOCGPHYS(sizeof(phys)),phys) < 0) {
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

  memset(events, 0, sizeof(events));
  if (ioctl(fd, EVIOCGBIT(0, EV_MAX), events) < 0) {
    LOG(ERROR) << "Error in powerm ioctl - event list";
    return NULL;
  }

  GIOChannel* channel = NULL;
  if (IS_BIT_SET(EV_KEY, events)) {
    unsigned long keys[NBITS(KEY_MAX)];
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
    unsigned long sw[NBITS(SW_LID + 1)];
    memset(sw, 0, sizeof(sw));
    if (ioctl(fd, EVIOCGBIT(EV_SW, SW_LID + 1), sw) < 0) {
      LOG(ERROR) << "Error in powerm ioctl - sw";
    }
    // An input event may have more than one kind of key or switch.
    // For example, if both the power button and the lid switch are handled
    // by the gpio_keys driver, both will share a single event in /dev/input.
    // In this case, only create one io channel per fd, and only add one
    // watch per event file.
    if (IS_BIT_SET(SW_LID, sw)) {
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
    if (strncmp(kEventBasename, sysname, strlen(kEventBasename)) == 0) {
      if (strcmp("add", action) == 0)
        input->AddEvent(sysname);
      else if (strcmp("remove", action) == 0)
        input->RemoveEvent(sysname);
    } else if (strncmp(kInputBasename, sysname, strlen(kInputBasename)) == 0) {
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
  if (!udev_monitor_ ) {
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
  unsigned int fd, rd, i;
  struct input_event ev[64];
  fd = g_io_channel_unix_get_fd(source);
  rd = read(fd, ev, sizeof(struct input_event) * 64);
  if (rd < (int) sizeof(struct input_event)) {
    LOG(ERROR) << "failed reading";
    return true;
  }
  if (!input->handler_)
    return true;
  for (i = 0; i < rd / sizeof(struct input_event); i++) {
    if (EV_KEY == ev[i].type && KEY_POWER == ev[i].code)
      (*input->handler_)(input->handler_data_, PWRBUTTON, ev[i].value);
    else if (EV_KEY == ev[i].type && KEY_F13 == ev[i].code)
      (*input->handler_)(input->handler_data_, LOCKBUTTON, ev[i].value);
    else if (EV_SW == ev[i].type && SW_LID == ev[i].code)
      (*input->handler_)(input->handler_data_, LID, ev[i].value);
    else if (EV_KEY == ev[i].type && KEY_F4 == ev[i].code)
      (*input->handler_)(input->handler_data_, KEYF4, ev[i].value);
    else if (EV_KEY == ev[i].type && KEY_LEFTCTRL == ev[i].code)
      (*input->handler_)(input->handler_data_, KEYLEFTCTRL, ev[i].value);
    else if (EV_KEY == ev[i].type && KEY_RIGHTCTRL == ev[i].code)
      (*input->handler_)(input->handler_data_, KEYRIGHTCTRL, ev[i].value);
  }
  return true;
}

void Input::RegisterHandler(InputHandler handler, void* data) {
  handler_ = handler;
  handler_data_ = data;
}

} // namespace power_manager
