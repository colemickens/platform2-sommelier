// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "permission_broker/deny_claimed_usb_device_rule.h"

#include <libudev.h>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "permission_broker/udev_scopers.h"

using policy::DevicePolicy;

namespace permission_broker {

DenyClaimedUsbDeviceRule::DenyClaimedUsbDeviceRule()
    : UsbSubsystemUdevRule("DenyClaimedUsbDeviceRule"),
      policy_loaded_(false) {
}

DenyClaimedUsbDeviceRule::~DenyClaimedUsbDeviceRule() {
}

bool DenyClaimedUsbDeviceRule::LoadPolicy() {
  usb_whitelist_.clear();

  auto policy_provider = base::MakeUnique<policy::PolicyProvider>();
  policy_provider->Reload();

  // No available policies.
  if (!policy_provider->device_policy_is_loaded())
    return false;

  const policy::DevicePolicy* policy = &policy_provider->GetDevicePolicy();
  return policy->GetUsbDetachableWhitelist(&usb_whitelist_);
}

bool DenyClaimedUsbDeviceRule::IsDeviceDetachable(udev_device* device) {
  // Retrieve the device policy for detachable USB devices if needed.
  if (!policy_loaded_)
    policy_loaded_ = LoadPolicy();
  if (!policy_loaded_)
    return false;

  // Check whether this USB device is whitelisted
  const char *str_vid = udev_device_get_sysattr_value(device, "idVendor");
  const char *str_pid = udev_device_get_sysattr_value(device, "idProduct");
  unsigned vendor_id, product_id;
  if (!str_vid || !base::HexStringToUInt(str_vid, &vendor_id))
    return false;
  if (!str_pid || !base::HexStringToUInt(str_pid, &product_id))
    return false;

  for (const DevicePolicy::UsbDeviceId &id : usb_whitelist_) {
    if (id.vendor_id == vendor_id &&
        (!id.product_id || id.product_id == product_id))
        return true;
  }

  return false;
}

Rule::Result DenyClaimedUsbDeviceRule::ProcessUsbDevice(udev_device* device) {
  const char* device_syspath = udev_device_get_syspath(device);
  if (!device_syspath) {
    return DENY;
  }

  udev* udev = udev_device_get_udev(device);
  ScopedUdevEnumeratePtr enumerate(udev_enumerate_new(udev));
  udev_enumerate_scan_devices(enumerate.get());

  bool found_claimed_interface = false;
  bool found_unclaimed_interface = false;
  struct udev_list_entry *entry = NULL;
  udev_list_entry_foreach(entry,
                          udev_enumerate_get_list_entry(enumerate.get())) {
    const char *entry_path = udev_list_entry_get_name(entry);
    ScopedUdevDevicePtr child(udev_device_new_from_syspath(udev, entry_path));

    // Find out if this entry's direct parent is the device in question.
    struct udev_device* parent = udev_device_get_parent(child.get());
    if (!parent) {
      continue;
    }
    const char* parent_syspath = udev_device_get_syspath(parent);
    if (!parent_syspath || strcmp(device_syspath, parent_syspath) != 0) {
      continue;
    }

    const char* child_type = udev_device_get_devtype(child.get());
    if (!child_type || strcmp(child_type, "usb_interface") != 0) {
      // If this is not a usb_interface node then something is wrong, fail safe.
      return DENY;
    }

    const char* driver = udev_device_get_driver(child.get());
    if (driver) {
      found_claimed_interface = true;
    } else {
      found_unclaimed_interface = true;
    }
  }

  if (found_claimed_interface) {
    if (IsDeviceDetachable(device))
      return ALLOW_WITH_DETACH;
    else
      return found_unclaimed_interface ? ALLOW_WITH_LOCKDOWN : DENY;
  } else {
    return IGNORE;
  }
}

}  // namespace permission_broker
