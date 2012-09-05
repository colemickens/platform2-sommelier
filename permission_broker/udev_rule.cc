// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "permission_broker/udev_rule.h"

#include <libudev.h>

#include <string>

using std::string;

namespace permission_broker {

UdevRule::UdevRule(const string &name)
    : Rule(name), udev_(udev_new()), enumerate_(udev_enumerate_new(udev_)) {}

UdevRule::~UdevRule() {
  udev_enumerate_unref(enumerate_);
  udev_unref(udev_);
}

Rule::Result UdevRule::Process(const string &path) {
  udev_enumerate_scan_devices(enumerate_);

  struct udev_list_entry *entry = NULL;
  udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(enumerate_)) {
    const char *const syspath = udev_list_entry_get_name(entry);
    struct udev_device *device = udev_device_new_from_syspath(udev_, syspath);

    const char *const devnode = udev_device_get_devnode(device);
    if (devnode && !strcmp(devnode, path.c_str())) {
      const Result result = ProcessDevice(device);
      udev_device_unref(device);
      return result;
    }

    udev_device_unref(device);
  }

  return IGNORE;
}

}  // namespace permission_broker
