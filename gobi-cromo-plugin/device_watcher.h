// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef PLUGIN_DEVICE_WATCHER_H_
#define PLUGIN_DEVICE_WATCHER_H_

#include <glib.h>

// Use udev to keep track of additions and removals of devices
struct udev;
struct udev_monitor;
struct udev_device;

typedef void (*DeviceCallback)(void*);
typedef void (*TimeoutCallback)(void*);

class DeviceWatcher {
 public:
  DeviceWatcher();
  ~DeviceWatcher();

  void StartMonitoring(const char* subsystem);
  void StartPolling(int interval_secs,
                    TimeoutCallback callback,
                    void* userdata);
  void StopPolling();
  void HandleUdevEvent();
  void HandlePollEvent();

  void set_callback(DeviceCallback callback, void* userdata);

 private:
  DeviceCallback device_callback_;
  void* device_callback_arg_;
  TimeoutCallback timeout_callback_;
  void* timeout_callback_arg_;
  struct udev* udev_;
  struct udev_monitor* udev_monitor_;
  guint udev_watch_id_;
  guint timeout_id_;
};

#endif  // PLUGIN_DEVICE_WATCHER_H_
