// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <libudev.h>

#include <set>
#include <string>

#include "base/logging.h"
#include "permission_broker/deny_claimed_usb_device_rule.h"

using std::set;
using std::string;

namespace permission_broker {

class DenyClaimedUsbDeviceRuleTest : public testing::Test {
 public:
  DenyClaimedUsbDeviceRuleTest() {}
  virtual ~DenyClaimedUsbDeviceRuleTest() {}

 protected:
  void FindClaimedDevices(set<string> *paths) {
    paths->clear();

    struct udev *udev = udev_new();
    struct udev_enumerate *enumerate = udev_enumerate_new(udev);
    udev_enumerate_scan_devices(enumerate);

    struct udev_list_entry *entry = NULL;
    udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(enumerate)) {
      const char *syspath = udev_list_entry_get_name(entry);
      struct udev_device *device = udev_device_new_from_syspath(udev, syspath);

      struct udev_device *parent =
          udev_device_get_parent_with_subsystem_devtype(
              device, "usb", "usb_device");
      if (parent) {
        // Hubs do not appear as claimed devices to this rule, so we must
        // exclude them from the check.
        if (string("09") != udev_device_get_sysattr_value(parent,
                                                          "bDeviceClass"))
          paths->insert(udev_device_get_devnode(parent));
      }

      udev_device_unref(device);
    }

    udev_enumerate_unref(enumerate);
    udev_unref(udev);
  }

  DenyClaimedUsbDeviceRule rule_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DenyClaimedUsbDeviceRuleTest);
};

TEST_F(DenyClaimedUsbDeviceRuleTest, IgnoreNonUsbDevice) {
  ASSERT_EQ(Rule::IGNORE, rule_.Process("/dev/tty0"));
}

TEST_F(DenyClaimedUsbDeviceRuleTest, IgnoreUnclaimedUsbDevice) {
  ASSERT_EQ(Rule::IGNORE, rule_.Process("/dev/bus/usb/001/001"));
}

TEST_F(DenyClaimedUsbDeviceRuleTest, DenyClaimedUsbDevice) {
  set<string> claimed_usb_devices;
  FindClaimedDevices(&claimed_usb_devices);
  if (claimed_usb_devices.empty()) {
    LOG(WARNING) << "This test requires at least one claimed USB device to be "
                 << "attached to the system. It is peculiar that a modern "
                 << "computer should appear to have no such devices.";
    FAIL();
  }

  for (set<string>::const_iterator i = claimed_usb_devices.begin(); i !=
       claimed_usb_devices.end(); ++i)
    ASSERT_EQ(Rule::DENY, rule_.Process(*i)) << *i;
}

}  // namespace permission_broker
