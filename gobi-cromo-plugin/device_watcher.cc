// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device_watcher.h"

extern "C" {
#include <libudev.h>
}

#include <base/logging.h>

static gboolean udev_event(GIOChannel* channel,
                           GIOCondition condition,
                           gpointer user_data) {
  DeviceWatcher* dw = static_cast<DeviceWatcher*>(user_data);
  dw->HandleUdevEvent();
  return TRUE;
}

static gboolean timeout_event(gpointer data) {
  DeviceWatcher* dw = static_cast<DeviceWatcher*>(data);
  dw->HandlePollEvent();
  return TRUE;
}

DeviceWatcher::DeviceWatcher(const char* subsystem)
  : subsystem_(subsystem),
    device_callback_(NULL),
    timeout_callback_(NULL),
    udev_(NULL),
    udev_monitor_(NULL),
    udev_watch_id_(0),
    timeout_id_(0) {
}

DeviceWatcher::~DeviceWatcher() {
  StopMonitoring();
}

void DeviceWatcher::StartMonitoring() {
  if (udev_ != NULL) {
    LOG(WARNING) << "StartMonitoring called with monitoring already in effect";
    return;
  }
  udev_ = udev_new();
  if (udev_ == NULL) {
    LOG(WARNING) << "Failed to create udev context";
    return;
  }
  udev_monitor_ = udev_monitor_new_from_netlink(udev_, "udev");
  if (udev_monitor_ == NULL) {
    LOG(WARNING) << "Failed to create udev monitor";
    udev_unref(udev_);
    udev_ = NULL;
    return;
  }
  int rc = udev_monitor_filter_add_match_subsystem_devtype(udev_monitor_,
                                                           subsystem_.c_str(),
                                                           NULL);
  if (rc != 0) {
    LOG(WARNING) << "Failed to add udev_monitor subsystem filter: " << rc;
    udev_monitor_unref(udev_monitor_);
    udev_unref(udev_);
    udev_ = NULL;
    udev_monitor_ = NULL;
  }
  rc = udev_monitor_enable_receiving(udev_monitor_);
  if (rc != 0) {
    LOG(WARNING) << "Failed to start udev monitoring: " << rc;
    udev_monitor_unref(udev_monitor_);
    udev_unref(udev_);
    udev_ = NULL;
    udev_monitor_ = NULL;
  }
  int fd = udev_monitor_get_fd(udev_monitor_);

  GIOChannel* iochan = g_io_channel_unix_new(fd);
  GError *gerror = NULL;
  g_io_channel_set_encoding(iochan, NULL, &gerror);
  g_io_channel_set_buffered(iochan, FALSE);
  udev_watch_id_ = g_io_add_watch(iochan, G_IO_IN, udev_event, this);
  g_io_channel_unref(iochan);
}

void DeviceWatcher::StopMonitoring() {
  if (udev_watch_id_ != 0) {
    g_source_remove(udev_watch_id_);
    udev_watch_id_ = 0;
  }
  if (udev_monitor_ != NULL) {
    udev_monitor_filter_remove(udev_monitor_);
    udev_monitor_unref(udev_monitor_);
    udev_monitor_ = NULL;
  }
  if (udev_ != NULL) {
    udev_unref(udev_);
    udev_ = NULL;
  }
}

void DeviceWatcher::StartPolling(int interval_secs,
                                 TimeoutCallback callback,
                                 void* userdata) {
  LOG(INFO) << "StartPolling(" << interval_secs << ")";
  timeout_callback_ = callback;
  timeout_callback_arg_ = userdata;
  timeout_id_ = g_timeout_add_seconds(interval_secs,
                                      timeout_event,
                                      this);
}

void DeviceWatcher::StopPolling() {
  if (timeout_id_ != 0) {
    LOG(INFO) << "StopPolling()";
    g_source_remove(timeout_id_);
    timeout_id_ = 0;
  }
}

static const char *NullFilter(const char *p) {
  return p ? p : "";
}

void DeviceWatcher::HandleUdevEvent() {
  struct udev_device* device = udev_monitor_receive_device(udev_monitor_);

  if (device == NULL) {
    LOG(WARNING) << "No device from receive_device";
    return;
  }
  LOG(INFO) << "udev: "
            << "  Action: " << NullFilter(udev_device_get_action(device))
            << "  Node: " << NullFilter(udev_device_get_devnode(device))
            << "  Subsystem: " << NullFilter(udev_device_get_subsystem(device))
            << "  Devtype: " << NullFilter(udev_device_get_devtype(device))
            << "  Driver: " << NullFilter(udev_device_get_driver(device));
  if (device_callback_ != NULL) {
    device_callback_(device_callback_arg_,
                     udev_device_get_action(device),
                     udev_device_get_devnode(device));
  }
  udev_device_unref(device);
}

void DeviceWatcher::HandlePollEvent() {
  if (timeout_callback_ != NULL)
    timeout_callback_(timeout_callback_arg_);
}

void DeviceWatcher::set_callback(DeviceCallback callback, void* userdata) {
  device_callback_ = callback;
  device_callback_arg_ = userdata;
}
