// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "permission_broker/deny_claimed_usb_device_rule.h"

#include <libudev.h>

#include <memory>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "permission_broker/udev_scopers.h"

using policy::DevicePolicy;

namespace {

const uint32_t kAdbClass = 0xff;
const uint32_t kAdbSubclass = 0x42;
const uint32_t kAdbProtocol = 0x1;

bool GetUIntSysattr(udev_device* device, const char* key, uint32_t* val) {
  CHECK(val);

  const char *str_val = udev_device_get_sysattr_value(device, key);
  return str_val && base::HexStringToUInt(str_val, val);
}

}  // namespace

namespace permission_broker {

DenyClaimedUsbDeviceRule::DenyClaimedUsbDeviceRule()
    : UsbSubsystemUdevRule("DenyClaimedUsbDeviceRule"), policy_loaded_(false) {}

DenyClaimedUsbDeviceRule::~DenyClaimedUsbDeviceRule() {}

bool DenyClaimedUsbDeviceRule::LoadPolicy() {
  usb_whitelist_.clear();

  auto policy_provider = std::make_unique<policy::PolicyProvider>();
  policy_provider->Reload();

  // No available policies.
  if (!policy_provider->device_policy_is_loaded())
    return false;

  const policy::DevicePolicy* policy = &policy_provider->GetDevicePolicy();
  return policy->GetUsbDetachableWhitelist(&usb_whitelist_);
}

bool DenyClaimedUsbDeviceRule::IsDeviceDetachableByPolicy(udev_device* device) {
  // Retrieve the device policy for detachable USB devices if needed.
  if (!policy_loaded_)
    policy_loaded_ = LoadPolicy();
  if (!policy_loaded_)
    return false;

  // Check whether this USB device is whitelisted.
  uint32_t vendor_id, product_id;
  if (!GetUIntSysattr(device, "idVendor", &vendor_id) ||
      !GetUIntSysattr(device, "idProduct", &product_id))
    return false;

  for (const DevicePolicy::UsbDeviceId& id : usb_whitelist_) {
    if (id.vendor_id == vendor_id &&
        (!id.product_id || id.product_id == product_id))
      return true;
  }

  return false;
}

bool DenyClaimedUsbDeviceRule::IsInterfaceAdb(udev_device* device) {
  uint32_t intf_class, intf_subclass, intf_protocol;
  if (!GetUIntSysattr(device, "bInterfaceClass", &intf_class) ||
      !GetUIntSysattr(device, "bInterfaceSubClass", &intf_subclass) ||
      !GetUIntSysattr(device, "bInterfaceProtocol", &intf_protocol))
    return false;

  return intf_class == kAdbClass && intf_subclass == kAdbSubclass &&
         intf_protocol == kAdbProtocol;
}

bool IsDeviceAllowedSerial(udev_device* device) {
  // Check whether this USB device is from Arduino.
  const uint32_t kArduinoVendorId = 0x2341;
  uint32_t vendor_id;
  if (!GetUIntSysattr(device, "idVendor", &vendor_id))
    return false;
  return vendor_id == kArduinoVendorId;
}

Rule::Result DenyClaimedUsbDeviceRule::ProcessUsbDevice(udev_device* device) {
  const char *device_syspath = udev_device_get_syspath(device);
  if (!device_syspath) {
    return DENY;
  }

  udev* udev = udev_device_get_udev(device);
  ScopedUdevEnumeratePtr enumerate(udev_enumerate_new(udev));
  udev_enumerate_scan_devices(enumerate.get());

  bool found_claimed_interface = false;
  bool found_unclaimed_interface = false;
  bool found_adb_interface = false;
  struct udev_list_entry *entry = nullptr;
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
    found_adb_interface |= IsInterfaceAdb(child.get());
  }

  if (found_claimed_interface) {
    if (IsDeviceDetachableByPolicy(device) || IsDeviceAllowedSerial(device) ||
        found_adb_interface)
      return ALLOW_WITH_DETACH;
    else
      return found_unclaimed_interface ? ALLOW_WITH_LOCKDOWN : DENY;
  } else {
    return IGNORE;
  }
}

}  // namespace permission_broker
