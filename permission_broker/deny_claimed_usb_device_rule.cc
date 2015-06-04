// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "permission_broker/deny_claimed_usb_device_rule.h"

#include <libudev.h>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "permission_broker/udev_scopers.h"

namespace permission_broker {

DenyClaimedUsbDeviceRule::DenyClaimedUsbDeviceRule()
    : Rule("DenyClaimedUsbDeviceRule"), udev_(udev_new()) {}

DenyClaimedUsbDeviceRule::~DenyClaimedUsbDeviceRule() {
  udev_unref(udev_);
}

Rule::Result DenyClaimedUsbDeviceRule::Process(const std::string& path) {
  ScopedUdevEnumeratePtr enumerate(udev_enumerate_new(udev_));
  udev_enumerate_scan_devices(enumerate.get());

  bool found_claimed_interface = false;
  bool found_unclaimed_interface = false;
  struct udev_list_entry *entry = NULL;
  udev_list_entry_foreach(entry,
                          udev_enumerate_get_list_entry(enumerate.get())) {
    const char *entry_path = udev_list_entry_get_name(entry);
    ScopedUdevDevicePtr device(udev_device_new_from_syspath(udev_, entry_path));

    // Find out if this entry's direct parent is a usb_device matching the path.
    struct udev_device* parent = udev_device_get_parent(device.get());
    if (!parent) {
      continue;
    }
    const char* parent_subsystem = udev_device_get_subsystem(parent);
    const char* parent_device_type = udev_device_get_devtype(parent);
    if (!parent_subsystem || strcmp(parent_subsystem, "usb") != 0 ||
        !parent_device_type || strcmp(parent_device_type, "usb_device") != 0 ||
        path != udev_device_get_devnode(parent)) {
      continue;
    }

    const char* device_type = udev_device_get_devtype(device.get());
    if (!device_type || strcmp(device_type, "usb_interface") != 0) {
      // If this is not a usb_interface node then something is wrong, fail safe.
      return DENY;
    }

    const char* driver = udev_device_get_driver(device.get());
    if (driver) {
      found_claimed_interface = true;
    } else {
      found_unclaimed_interface = true;
    }
  }

  if (found_claimed_interface) {
    return found_unclaimed_interface ? ALLOW_WITH_LOCKDOWN : DENY;
  } else {
    return IGNORE;
  }
}

}  // namespace permission_broker
