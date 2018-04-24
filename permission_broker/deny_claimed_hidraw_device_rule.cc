// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "permission_broker/deny_claimed_hidraw_device_rule.h"

#include <libudev.h>
#include <linux/input.h>

#include <limits>
#include <string>
#include <vector>

#include "base/containers/adapters.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "permission_broker/udev_scopers.h"

namespace permission_broker {

namespace {

const char kLogitechUnifyingReceiverDriver[] = "logitech-djreceiver";

// The kernel expresses capabilities as a bitfield, broken into long-sized
// chunks encoded in hexadecimal.
bool ParseInputCapabilities(const char* input, std::vector<uint64_t>* output) {
  std::vector<std::string> chunks =
      base::SplitString(input, " ", base::KEEP_WHITESPACE,
                        base::SPLIT_WANT_ALL);

  output->clear();
  output->reserve(chunks.size());

  // The most-significant chunk of the bitmask is stored first, iterate over
  // the chunks in reverse so that the result is easier to work with.
  for (const std::string& chunk : base::Reversed(chunks)) {
    uint64_t value = 0;
    if (!base::HexStringToUInt64(chunk, &value)) {
      LOG(ERROR) << "Failed to parse: " << chunk;
      return false;
    }
    // NOLINTNEXTLINE(runtime/int)
    if (value > std::numeric_limits<unsigned long>::max()) {
      LOG(ERROR) << "Chunk value too large for platform: " << value;
      return false;
    }
    output->push_back(value);
  }

  return true;
}

bool IsCapabilityBitSet(const std::vector<uint64_t>& bitfield, size_t bit) {
  size_t offset = bit / (sizeof(long) * 8);  // NOLINT(runtime/int)
  if (offset >= bitfield.size()) {
    return false;
  }
  // NOLINTNEXTLINE(runtime/int)
  return bitfield[offset] & (1ULL << (bit - offset * sizeof(long) * 8));
}

bool IsCfmDevice() {
#if defined(USE_CFM_ENABLED_DEVICE)
  return true;
#else
  return false;
#endif
}

}  // namespace

DenyClaimedHidrawDeviceRule::DenyClaimedHidrawDeviceRule()
    : HidrawSubsystemUdevRule("DenyClaimedHidrawDeviceRule") {}

Rule::Result DenyClaimedHidrawDeviceRule::ProcessHidrawDevice(
    struct udev_device* device) {
  // Add an exception to the rule for Logitech Unifying receiver.
  // This hidraw device is a parent of devices that have input
  // subsystem. Yet the traffic to those children is not available on
  // the hidraw node of the receiver, so it is safe to white-list it.
  struct udev_device* hid_parent =
      udev_device_get_parent_with_subsystem_devtype(device, "hid", nullptr);
  if (!hid_parent) {
    // A hidraw device without a HID parent, we don't know what this can be.
    return DENY;
  }

  const char* hid_parent_driver = udev_device_get_driver(hid_parent);
  if (hid_parent_driver &&
      strcmp(hid_parent_driver, kLogitechUnifyingReceiverDriver) == 0) {
    LOG(INFO) << "Found Logitech Unifying receiver. Skipping rule.";
    return IGNORE;
  }

  std::string hid_parent_path(udev_device_get_syspath(hid_parent));
  std::string usb_interface_path;
  struct udev_device* usb_interface =
      udev_device_get_parent_with_subsystem_devtype(
          device, "usb", "usb_interface");

  if (usb_interface)
    usb_interface_path = udev_device_get_syspath(usb_interface);

  // Count the number of children of the same HID parent as us.
  int hid_siblings = 0;
  // Scan all children of the USB interface for subsystems other than generic
  // USB or HID, and all the children of the same HID parent device.
  // The presence of such subsystems is an indication that the device is in
  // use by another driver.
  //
  // Because udev lacks the ability to filter an enumeration by arbitrary
  // ancestor properties (e.g. "enumerate all nodes with a usb_interface
  // ancestor") we have to scan the entire set of devices to find potential
  // matches.
  struct udev* udev = udev_device_get_udev(device);
  ScopedUdevEnumeratePtr enumerate(udev_enumerate_new(udev));
  udev_enumerate_scan_devices(enumerate.get());
  struct udev_list_entry* entry;
  udev_list_entry_foreach(entry,
                          udev_enumerate_get_list_entry(enumerate.get())) {
    const char* syspath = udev_list_entry_get_name(entry);
    ScopedUdevDevicePtr child(udev_device_new_from_syspath(udev, syspath));
    struct udev_device* child_usb_interface =
        udev_device_get_parent_with_subsystem_devtype(
            child.get(), "usb", "usb_interface");
    struct udev_device* child_hid_parent =
        udev_device_get_parent_with_subsystem_devtype(
            child.get(), "hid", nullptr);
    if (!child_usb_interface && !child_hid_parent) {
      continue;
    }
    // This device shares a USB interface with the hidraw device in question.
    // Check its subsystem to see if it should block hidraw access.
    if (usb_interface && child_usb_interface &&
        usb_interface_path == udev_device_get_syspath(child_usb_interface) &&
        ShouldSiblingSubsystemExcludeHidAccess(child.get())) {
      return DENY;
    }
    // This device shares the same HID device as parent, count it.
    if (child_hid_parent &&
        hid_parent_path == udev_device_get_syspath(child_hid_parent)) {
      hid_siblings++;
    }
  }

  // If the underlying HID device presents no other interface than hidraw,
  // we can use it.
  // USB devices have already been filtered directly in the loop above.
  if (!usb_interface && hid_siblings != 1)
    return DENY;

  return IGNORE;
}

bool DenyClaimedHidrawDeviceRule::ShouldSiblingSubsystemExcludeHidAccess(
    struct udev_device* sibling) {
  const char* subsystem = udev_device_get_subsystem(sibling);
  if (!subsystem) {
    return false;
  }

  // Generic USB/HID is okay.
  if (strcmp(subsystem, "hid") == 0 ||
      strcmp(subsystem, "hidraw") == 0 ||
      (IsCfmDevice() && strcmp(subsystem, "leds") == 0) ||
      strcmp(subsystem, "usb") == 0 ||
      strcmp(subsystem, "usbmisc") == 0) {
    return false;
  }

  if (strcmp(subsystem, "input") == 0 &&
      !ShouldInputCapabilitiesExcludeHidAccess(
          udev_device_get_sysattr_value(sibling, "capabilities/abs"),
          udev_device_get_sysattr_value(sibling, "capabilities/rel"),
          udev_device_get_sysattr_value(sibling, "capabilities/key"))) {
    return false;
  }

  return true;
}

bool DenyClaimedHidrawDeviceRule::ShouldInputCapabilitiesExcludeHidAccess(
    const char* abs_capabilities,
    const char* rel_capabilities,
    const char* key_capabilities) {
  std::vector<uint64_t> capabilities;
  if (abs_capabilities) {
    if (!ParseInputCapabilities(abs_capabilities, &capabilities)) {
      // Parse error? Fail safe.
      return true;
    }
    for (uint64_t value : capabilities) {
      if (value != 0) {
        // Any absolute pointer capabilities exclude access.
        return true;
      }
    }
  }

  if (rel_capabilities) {
    if (!ParseInputCapabilities(rel_capabilities, &capabilities)) {
      // Parse error? Fail safe.
      return true;
    }
    for (uint64_t value : capabilities) {
      if (value != 0) {
        // Any relative pointer capabilities exclude access.
        return true;
      }
    }
  }

  if (key_capabilities) {
    if (!ParseInputCapabilities(key_capabilities, &capabilities)) {
      // Parse error? Fail safe.
      return true;
    }
    // Any key code <= KEY_KPDOT (83) excludes access.
    for (int key = 0; key <= KEY_KPDOT; key++) {
      if (IsCapabilityBitSet(capabilities, key)) {
        return true;
      }
    }
    // Braille dots are outside the "normal keyboard keys" range.
    for (int key = KEY_BRL_DOT1; key <= KEY_BRL_DOT10; key++) {
      if (IsCapabilityBitSet(capabilities, key)) {
        return true;
      }
    }
  }

  return false;
}

}  // namespace permission_broker
