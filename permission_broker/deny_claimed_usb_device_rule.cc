// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "permission_broker/deny_claimed_usb_device_rule.h"

#include <libudev.h>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"

namespace permission_broker {

DenyClaimedUsbDeviceRule::DenyClaimedUsbDeviceRule()
    : Rule("DenyClaimedUsbDeviceRule"), udev_(udev_new()) {}

DenyClaimedUsbDeviceRule::~DenyClaimedUsbDeviceRule() {
  udev_unref(udev_);
}

Rule::Result DenyClaimedUsbDeviceRule::Process(const std::string &path,
                                               int interface_id) {
  struct udev_enumerate *enumerate = udev_enumerate_new(udev_);
  udev_enumerate_scan_devices(enumerate);

  bool deny = false;
  struct udev_list_entry *entry = NULL;
  udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(enumerate)) {
    const char *entry_path = udev_list_entry_get_name(entry);
    struct udev_device *device = udev_device_new_from_syspath(udev_,
                                                              entry_path);

    // usb_interface entries (direct descendants of usb_device entries) are not,
    // for the purposes of this rule, considered as having claimed the device.
    const char *device_type = udev_device_get_devtype(device);
    if (device_type &&
        !strcmp(device_type, "usb_interface") &&
        interface_id == ANY_INTERFACE) {
      udev_device_unref(device);
      continue;
    }

    struct udev_device *parent = udev_device_get_parent_with_subsystem_devtype(
        device, "usb", "usb_device");
    if (parent && (path == udev_device_get_devnode(parent))) {
      deny = true;
      // Check if the specific interface is already claimed (has driver).
      if (interface_id != ANY_INTERFACE) {
        const char* driver = udev_device_get_driver(device);
        if (!driver) {
          const char* interface_num = udev_device_get_sysattr_value(
              device, "bInterfaceNumber");
          int interface_value;
          if (interface_num &&
              base::HexStringToInt(interface_num, &interface_value) &&
              interface_value == interface_id) {
            deny = false;
            udev_device_unref(device);
            break;
          }
        }
      }
    }
    udev_device_unref(device);
  }

  udev_enumerate_unref(enumerate);

  if (deny)
    return DENY;
  return IGNORE;
}

}  // namespace permission_broker
