// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "permission_broker/deny_claimed_hidraw_device_rule.h"

#include <gtest/gtest.h>
#include <libudev.h>

#include <string>

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
  struct udev_list_entry* entry = nullptr;
  udev_list_entry_foreach(entry,
                          udev_enumerate_get_list_entry(enumerate.get())) {
    const char* syspath = udev_list_entry_get_name(entry);
    ScopedUdevDevicePtr device(
        udev_device_new_from_syspath(udev_.get(), syspath));
    Rule::Result result = rule_.ProcessHidrawDevice(device.get());

    // This device was ignored by the rule. Make sure that it's a USB device
    // and that it's USB interface is not, in fact, being used by other drivers.
    if (result == Rule::IGNORE) {
      struct udev_device* usb_interface =
          udev_device_get_parent_with_subsystem_devtype(
              device.get(), "usb", "usb_interface");
      struct udev_device* hid_parent =
          udev_device_get_parent_with_subsystem_devtype(
              device.get(), "hid", nullptr);

      ASSERT_NE(nullptr, hid_parent)
          << "We don't support hidraw devices with an HID parent.";

      std::string hid_parent_path(udev_device_get_syspath(hid_parent));
      std::string usb_interface_path;
      if (usb_interface)
        usb_interface_path.assign(udev_device_get_syspath(usb_interface));

      int hid_siblings = 0;
      // Verify that this hidraw device does not share a USB interface with any
      // other drivers' devices. This means we have to enumerate every device
      // to find any with the same ancestral usb_interface, then test for a non-
      // generic subsystem.
      ScopedUdevEnumeratePtr other_enumerate(udev_enumerate_new(udev_.get()));
      udev_enumerate_scan_devices(other_enumerate.get());
      struct udev_list_entry* other_entry = nullptr;
      udev_list_entry_foreach(
          other_entry,
          udev_enumerate_get_list_entry(other_enumerate.get())) {
        const char* other_path = udev_list_entry_get_name(other_entry);
        ScopedUdevDevicePtr other_device(
            udev_device_new_from_syspath(udev_.get(), other_path));
        struct udev_device* other_hid_parent =
            udev_device_get_parent_with_subsystem_devtype(
                other_device.get(), "hid", nullptr);
        if (other_hid_parent) {
          std::string other_hid_parent_path(
              udev_device_get_syspath(other_hid_parent));
          if (hid_parent_path == other_hid_parent_path) {
            hid_siblings++;
          }
        }
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
                  ShouldSiblingSubsystemExcludeHidAccess(other_device.get()))
              << "This rule should IGNORE claimed devices.";
        }
      }
      ASSERT_FALSE(!usb_interface && hid_siblings > 1)
          << "This rule should DENY all non-USB HID devices.";

    } else if (result != Rule::DENY) {
      FAIL() << "This rule should only either IGNORE or DENY devices.";
    }
  }
}

TEST_F(DenyClaimedHidrawDeviceRuleTest, InputCapabilityExclusions) {
  const char* kKeyboardKeys;
  const char* kMouseKeys;
  const char* kHeadsetKeys;
  const char* kBrailleKeys;
  const char* kSpeakerphoneAbs;
  const char* kSpeakerphoneKeys;
  const char* kAbsoluteMouseAbs;

  // The size of these bitfield chunks is the width of a userspace long.
  switch (sizeof(long)) {  // NOLINT(runtime/int)
    case 4:
      kKeyboardKeys =
          "10000 00000007 ff9f207a c14057ff "
          "febeffdf ffefffff ffffffff fffffffe";
      kMouseKeys = "1f0000 0 0 0 0 0 0 0 0";
      kHeadsetKeys = "18000 178 0 8e0000 0 0 0";
      kBrailleKeys = "7fe0000 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0";
      kSpeakerphoneAbs = "100 0";
      kSpeakerphoneKeys = "1 10000000 0 0 c0000000 0 0";
      kAbsoluteMouseAbs = "100 3";
      break;
    case 8:
      kKeyboardKeys =
          "1000000000007 ff9f207ac14057ff febeffdfffefffff fffffffffffffffe";
      kMouseKeys = "1f0000 0 0 0 0";
      kHeadsetKeys = "18000 17800000000 8e000000000000 0";
      kBrailleKeys = "7fe000000000000 0 0 0 0 0 0 0";
      kSpeakerphoneAbs = "10000000000";
      kSpeakerphoneKeys = "1 1000000000000000 0 c000000000000000 0";
      kAbsoluteMouseAbs = "10000000003";
      break;
    default:
      FAIL() << "Unsupported platform long width.";
  }

  // Example capabilities from a real keyboard. Should be excluded.
  EXPECT_TRUE(
      DenyClaimedHidrawDeviceRule::ShouldInputCapabilitiesExcludeHidAccess(
          "0", "0", kKeyboardKeys));

  // Example capabilities from a real mouse. Should be excluded.
  EXPECT_TRUE(
      DenyClaimedHidrawDeviceRule::ShouldInputCapabilitiesExcludeHidAccess(
          "0", "103", kMouseKeys));

  // Example capabilities from a headset with some telephony buttons. Should not
  // be excluded.
  EXPECT_FALSE(
      DenyClaimedHidrawDeviceRule::ShouldInputCapabilitiesExcludeHidAccess(
          "0", "0", kHeadsetKeys));

  // A braille input device (made up). Should be excluded.
  EXPECT_TRUE(
      DenyClaimedHidrawDeviceRule::ShouldInputCapabilitiesExcludeHidAccess(
          "0", "0", kBrailleKeys));

  // Example capabilities from a speakerphone device which includes ABS_MISC
  // events. Should not be excluded.
  EXPECT_FALSE(
      DenyClaimedHidrawDeviceRule::ShouldInputCapabilitiesExcludeHidAccess(
          kSpeakerphoneAbs, "0", kSpeakerphoneKeys));

  // A absolute pointing device (made up) which includes ABS_MISC events.
  // Should be excluded.
  EXPECT_TRUE(
      DenyClaimedHidrawDeviceRule::ShouldInputCapabilitiesExcludeHidAccess(
          kAbsoluteMouseAbs, "0", kMouseKeys));
}

}  // namespace permission_broker
