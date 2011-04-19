// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/udev_controller.h"

#include "base/logging.h"

namespace power_manager {

UdevController::UdevController(UdevDelegate* delegate,
                           const std::string& subsystem)
    : subsystem_(subsystem),
      delegate_(delegate),
      udev_(NULL) {
}

UdevController::~UdevController() {
  if (udev_)
    udev_unref(udev_);
}

bool UdevController::Init() {
  // Create the udev object.
  udev_ = udev_new();
  if (!udev_) {
    LOG(ERROR) << "Can't create udev object.";
    return false;
  }

  // Create the udev monitor structure.
  monitor_ = udev_monitor_new_from_netlink(udev_, "udev");
  if (!monitor_ ) {
    LOG(ERROR) << "Can't create udev monitor.";
    udev_unref(udev_);
    return false;
  }
  udev_monitor_filter_add_match_subsystem_devtype(monitor_,
                                                  subsystem_.c_str(),
                                                  NULL);
  udev_monitor_enable_receiving(monitor_);

  int fd = udev_monitor_get_fd(monitor_);

  GIOChannel* channel = g_io_channel_unix_new(fd);
  g_io_add_watch(channel, G_IO_IN, &(UdevController::EventHandler), this);

  LOG(INFO) << "Udev controller waiting for events on subsystem "
            << subsystem_;

  return true;
}

gboolean UdevController::EventHandler(GIOChannel* source,
                                      GIOCondition condition,
                                      gpointer data) {
  UdevController* controller = static_cast<UdevController*>(data);

  struct udev_device* dev = udev_monitor_receive_device(controller->monitor_);
  if (dev) {
    LOG(INFO) << "Event on ("
              << udev_device_get_devnode(dev)
              << "|"
              << udev_device_get_subsystem(dev)
              << "|"
              << udev_device_get_devtype(dev)
              << ") Action "
              << udev_device_get_action(dev);
    udev_device_unref(dev);
    controller->delegate_->Run(source, condition);
  } else {
    LOG(ERROR) << "Can't get receive_device()";
    return false;
  }
  return true;
}

}  // namespace power_manager
