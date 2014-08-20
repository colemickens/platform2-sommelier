// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <libudev.h>

#include <string>

#include "permission_broker/deny_claimed_hidraw_device_rule.h"
#include "permission_broker/udev_scopers.h"

namespace permission_broker {

class DenyClaimedHidrawDeviceRuleTest : public testing::Test {
 public:
  DenyClaimedHidrawDeviceRuleTest() : udev_(udev_new()) {}

  ~DenyClaimedHidrawDeviceRuleTest() override = default;

 protected:
  ScopedUdevPtr udev_;
  DenyClaimedHidrawDeviceRule rule_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DenyClaimedHidrawDeviceRuleTest);
};

TEST_F(DenyClaimedHidrawDeviceRuleTest, DenyClaimedHidrawDevices) {
  // Run the rule on every available device and verify that it only ignores
  // unclaimed USB HID devices, denying the rest.
  ScopedUdevEnumeratePtr enumerate(udev_enumerate_new(udev_.get()));
  udev_enumerate_add_match_subsystem(enumerate.get(), "hidraw");
  udev_enumerate_scan_devices(enumerate.get());
  struct udev_list_entry* entry = NULL;
  udev_list_entry_foreach(entry,
                          udev_enumerate_get_list_entry(enumerate.get())) {
    const char *syspath = udev_list_entry_get_name(entry);
    ScopedUdevDevicePtr device(
        udev_device_new_from_syspath(udev_.get(), syspath));
    Rule::Result result = rule_.ProcessHidrawDevice(device.get());

    // This device was ignored by the rule. Make sure that it's a USB device
    // and that it's USB interface is not, in fact, being used by other drivers.
    if (result == Rule::IGNORE) {
      struct udev_device* usb_interface =
          udev_device_get_parent_with_subsystem_devtype(
              device.get(), "usb", "usb_interface");

      ASSERT_TRUE(usb_interface != NULL) <<
          "This rule should DENY all non-USB HID devices.";

      std::string usb_interface_path(udev_device_get_syspath(usb_interface));

      // Verify that this hidraw device does not share a USB interface with any
      // other drivers' devices. This means we have to enumerate every device
      // to find any with the same ancestral usb_interface, then test for a non-
      // generic subsystem.
      ScopedUdevEnumeratePtr other_enumerate(udev_enumerate_new(udev_.get()));
      udev_enumerate_scan_devices(other_enumerate.get());
      struct udev_list_entry* other_entry = NULL;
      udev_list_entry_foreach(
          other_entry,
          udev_enumerate_get_list_entry(other_enumerate.get())) {
        const char* other_path = udev_list_entry_get_name(other_entry);
        ScopedUdevDevicePtr other_device(
            udev_device_new_from_syspath(udev_.get(), other_path));
        struct udev_device* other_usb_interface =
            udev_device_get_parent_with_subsystem_devtype(
                other_device.get(), "usb", "usb_interface");
        if (!other_usb_interface) {
          continue;
        }
        std::string other_usb_interface_path(
            udev_device_get_syspath(other_usb_interface));
        if (usb_interface_path == other_usb_interface_path) {
          ASSERT_FALSE(
              DenyClaimedHidrawDeviceRule::
                  ShouldSiblingSubsystemExcludeHidAccess(
                      udev_device_get_subsystem(other_device.get()))) <<
                          "This rule should IGNORE claimed devices.";
        }
      }
    } else if (result != Rule::DENY) {
      FAIL() << "This rule should only either IGNORE or DENY devices.";
    }
  }
}

}  // namespace permission_broker
