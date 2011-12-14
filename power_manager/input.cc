// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/input.h"

#include <string>

#include <dirent.h>
#include <fcntl.h>
#include <gdk/gdk.h>
#include <linux/input.h>
#include <stdlib.h>

#include "base/file_path.h"
#include "base/logging.h"

using std::map;

namespace power_manager {

const char kInputUdevSubsystem[] = "input";

Input::Input()
    : handler_(NULL),
      handler_data_(NULL),
      lid_fd_(-1),
      num_power_key_events_(0),
      num_lid_events_(0) {}

Input::~Input() {
  // TODO(bleung) : track and close ALL handles that we have open, and shutdown
  // g_io_channels as well.
  if (lid_fd_ >= 0)
    close(lid_fd_);
}

bool Input::Init() {
  RegisterUdevEventHandler();
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

bool Input::AddEvent(const char * name) {
  int event_num = -1;
  if (strncmp("event", name, 5)) {
    return false;
  }

  event_num = atoi(name + 5);

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
  if (strncmp("event", name, 5)) {
    return false;
  }

  event_num = atoi(name + 5);
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
    LOG(INFO) << "Event on ("
              << udev_device_get_subsystem(dev)
              << ") Action "
              << udev_device_get_action(dev)
              << " sys name "
              << udev_device_get_sysname(dev);
    if (strcmp("add", action) == 0)
      input->AddEvent(udev_device_get_sysname(dev));
    else if (strcmp("remove", action) == 0)
      input->RemoveEvent(udev_device_get_sysname(dev));
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
  }
  return true;
}

void Input::RegisterHandler(InputHandler handler, void* data) {
  handler_ = handler;
  handler_data_ = data;
}

} // namespace power_manager
