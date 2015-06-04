// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "permission_broker/deny_claimed_usb_device_rule.h"

#include <gtest/gtest.h>
#include <libudev.h>

#include <set>
#include <string>

#include "base/logging.h"
#include "base/stl_util.h"
#include "permission_broker/udev_scopers.h"

using std::set;
using std::string;

namespace permission_broker {

class DenyClaimedUsbDeviceRuleTest : public testing::Test {
 public:
  DenyClaimedUsbDeviceRuleTest() = default;
  ~DenyClaimedUsbDeviceRuleTest() override = default;

 protected:
  void SetUp() override {
    ScopedUdevPtr udev(udev_new());
    ScopedUdevEnumeratePtr enumerate(udev_enumerate_new(udev.get()));
    udev_enumerate_scan_devices(enumerate.get());

    struct udev_list_entry *entry = NULL;
    udev_list_entry_foreach(entry,
                            udev_enumerate_get_list_entry(enumerate.get())) {
      const char *syspath = udev_list_entry_get_name(entry);
      ScopedUdevDevicePtr device(
          udev_device_new_from_syspath(udev.get(), syspath));
      EXPECT_TRUE(device.get());

      const char* devtype = udev_device_get_devtype(device.get());
      if (!devtype || strcmp(devtype, "usb_interface") != 0)
        continue;

      struct udev_device* parent = udev_device_get_parent(device.get());
      if (!parent)
        continue;

      const char* devnode = udev_device_get_devnode(parent);
      if (!devnode)
        continue;

      string path = devnode;
      if (!ContainsKey(partially_claimed_devices_, path)) {
        if (udev_device_get_driver(device.get())) {
          auto it = unclaimed_devices_.find(path);
          if (it == unclaimed_devices_.end()) {
            claimed_devices_.insert(path);
          } else {
            partially_claimed_devices_.insert(path);
            unclaimed_devices_.erase(it);
          }
        } else {
          auto it = claimed_devices_.find(path);
          if (it == claimed_devices_.end()) {
            unclaimed_devices_.insert(path);
          } else {
            partially_claimed_devices_.insert(path);
            claimed_devices_.erase(it);
          }
        }
      }
    }
  }

  DenyClaimedUsbDeviceRule rule_;
  set<string> claimed_devices_;
  set<string> unclaimed_devices_;
  set<string> partially_claimed_devices_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DenyClaimedUsbDeviceRuleTest);
};

TEST_F(DenyClaimedUsbDeviceRuleTest, IgnoreNonUsbDevice) {
  ASSERT_EQ(Rule::IGNORE, rule_.Process("/dev/tty0"));
}

TEST_F(DenyClaimedUsbDeviceRuleTest, DenyClaimedUsbDevice) {
  if (claimed_devices_.empty())
    LOG(WARNING) << "Tests incomplete because there are no claimed devices "
                 << "connected.";

  for (const string& device : claimed_devices_)
    EXPECT_EQ(Rule::DENY, rule_.Process(device)) << device;
}

TEST_F(DenyClaimedUsbDeviceRuleTest, IgnoreUnclaimedUsbDevice) {
  if (unclaimed_devices_.empty())
    LOG(WARNING) << "Tests incomplete because there are no unclaimed devices "
                 << "connected.";

  for (const string& device : unclaimed_devices_)
    EXPECT_EQ(Rule::IGNORE, rule_.Process(device)) << device;
}

TEST_F(DenyClaimedUsbDeviceRuleTest,
       AllowPartiallyClaimedUsbDeviceWithLockdown) {
  if (partially_claimed_devices_.empty())
    LOG(WARNING) << "Tests incomplete because there are no partially claimed "
                 << "devices connected.";

  for (const string& device : partially_claimed_devices_)
    EXPECT_EQ(Rule::ALLOW_WITH_LOCKDOWN, rule_.Process(device)) << device;
}

}  // namespace permission_broker
