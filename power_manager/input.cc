// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/input.h"

#include <dirent.h>
#include <fcntl.h>
#include <gdk/gdk.h>
#include <linux/input.h>

#include "base/file_path.h"
#include "base/logging.h"

namespace power_manager {

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
      if (result->d_name[0] && 0 == strncmp("event", result->d_name, 5)) {
        FilePath event_path = input_path.Append(result->d_name);
        int fd;
        LOG(INFO) << "Found input device : " << event_path.value().c_str();
        if (access(event_path.value().c_str(), R_OK)) {
          LOG(WARNING) << "Failed to read from device.";
          continue;
        }
        if ((fd = open(event_path.value().c_str(), O_RDONLY)) < 0) {
          LOG(ERROR) << "Failed to open - " << event_path.value().c_str();
          continue;
        }
        if (RegisterInputEvent(fd))
          num_registered++;
        else if (0 > close(fd)) // event not registered, closing.
          LOG(ERROR) << "Error closing file handle.";
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

bool Input::RegisterInputEvent(int fd) {
  unsigned long events[NBITS(EV_MAX)];
  char name[256] = "Unknown";
  char phys[256] = "Unknown";
  bool watch_added = false;

  if (ioctl(fd, EVIOCGNAME(sizeof(name)),name) < 0) {
    LOG(ERROR) << "Could not get name of this device.";
    return false;
  } else {
    LOG(INFO) << "Device name : " << name;
  }

  if (ioctl(fd, EVIOCGPHYS(sizeof(phys)),phys) < 0) {
    LOG(ERROR) << "Could not get topographic phys path of this device.";
    return false;
  } else {
    LOG(INFO) << "Device topo phys : " << phys;
  }

#ifdef NEW_POWER_BUTTON
  // Skip input events from the ACPI power button (identified as LNXPWRBN) if
  // a new power button is present. In that case, don't skip the built in
  // keyboard, which starts with isa in its topo phys path.
  if (0 == strncmp("LNXPWRBN", phys, 8) || 0 == strncmp("usb", phys, 3)) {
    LOG(INFO) << "Skipping interface : " << phys;
    return false;
}
#else
  // Skip input events that are either on a usb bus or on the built in keyboard.
  // Many of these devices advertise a power key but do not physically have one.
  // Skipping will reduce the wasteful waking of powerm due to keyboard events.
  if (0 == strncmp("isa", phys, 3) || 0 == strncmp("usb", phys, 3)) {
    LOG(INFO) << "Skipping interface : " << phys;
    return false;
  }
#endif

  memset(events, 0, sizeof(events));
  if (ioctl(fd, EVIOCGBIT(0, EV_MAX), events) < 0) {
    LOG(ERROR) << "Error in powerm ioctl - event list";
    return false;
  }

  if (IS_BIT_SET(EV_KEY, events)) {
    unsigned long keys[NBITS(KEY_MAX)];
    memset(keys, 0, sizeof(keys));
    if (ioctl(fd, EVIOCGBIT(EV_KEY, KEY_MAX), keys) < 0) {
      LOG(ERROR) << "Error in powerm ioctl - key";
    }
    if (IS_BIT_SET(KEY_POWER, keys)) {
      GIOChannel * channel;
      LOG(INFO) << "Watching this event for power button!";
      channel = g_io_channel_unix_new(fd);
      g_io_add_watch(channel, G_IO_IN, &(Input::EventHandler), this);
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
    if (IS_BIT_SET(SW_LID, sw)) {
      num_lid_events_++;
      if (!watch_added) {
          GIOChannel * channel;
          LOG(INFO) << "Watching this event for lid switch!";
          if (lid_fd_ >= 0)
            LOG(WARNING) << "Multiple lid events found on system!";
          lid_fd_ = fd;
          channel = g_io_channel_unix_new(fd);
          g_io_add_watch(channel, G_IO_IN, &(Input::EventHandler), this);
          watch_added = true;
      } else {
        LOG(INFO) << "Watched event also has a lid!";
      }
    }
  }
  return watch_added;
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
    return false;
  }
  if (!input->handler_)
    return true;
  for (i = 0; i < rd / sizeof(struct input_event); i++)
    if (EV_KEY == ev[i].type && KEY_POWER == ev[i].code)
      (*input->handler_)(input->handler_data_, PWRBUTTON, ev[i].value);
    else if (EV_SW == ev[i].type && SW_LID == ev[i].code)
      (*input->handler_)(input->handler_data_, LID, ev[i].value);
  return true;
}

void Input::RegisterHandler(InputHandler handler, void* data) {
  handler_ = handler;
  handler_data_ = data;
}

} // namespace power_manager
