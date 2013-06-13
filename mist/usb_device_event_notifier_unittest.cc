// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mist/usb_device_event_notifier.h"

#include <gtest/gtest.h>

#include "mist/event_dispatcher.h"
#include "mist/mock_udev.h"
#include "mist/mock_udev_device.h"
#include "mist/mock_udev_monitor.h"
#include "mist/mock_usb_device_event_observer.h"

using testing::Return;
using testing::_;

namespace mist {

namespace {

const int kFakeUdevMonitorFileDescriptor = 999;

const char kUdevActionAdd[] = "add";
const char kUdevActionChange[] = "change";
const char kUdevActionRemove[] = "remove";

const char kFakeUsbDevice1SysPath[] = "/sys/devices/fake/1";
const uint16 kFakeUsbDevice1VendorId = 0x0123;
const char kFakeUsbDevice1VendorIdString[] = "0123";
const uint16 kFakeUsbDevice1ProductId = 0x4567;
const char kFakeUsbDevice1ProductIdString[] = "4567";

const char kFakeUsbDevice2SysPath[] = "/sys/devices/fake/2";
const uint16 kFakeUsbDevice2VendorId = 0x89ab;
const char kFakeUsbDevice2VendorIdString[] = "89ab";
const uint16 kFakeUsbDevice2ProductId = 0xcdef;
const char kFakeUsbDevice2ProductIdString[] = "cdef";

}  // namespace

class UsbDeviceEventNotifierTest : public testing::Test {
 protected:
  UsbDeviceEventNotifierTest() : notifier_(&dispatcher_, &udev_) {}

  EventDispatcher dispatcher_;
  MockUdev udev_;
  MockUsbDeviceEventObserver observer_;
  UsbDeviceEventNotifier notifier_;
};

TEST_F(UsbDeviceEventNotifierTest, ConvertNullToEmptyString) {
  EXPECT_EQ("", UsbDeviceEventNotifier::ConvertNullToEmptyString(NULL));
  EXPECT_EQ("", UsbDeviceEventNotifier::ConvertNullToEmptyString(""));
  EXPECT_EQ("a", UsbDeviceEventNotifier::ConvertNullToEmptyString("a"));
  EXPECT_EQ("test string",
            UsbDeviceEventNotifier::ConvertNullToEmptyString("test string"));
}

TEST_F(UsbDeviceEventNotifierTest, ConvertIdStringToValue) {
  uint16 id = 0x0000;

  EXPECT_FALSE(UsbDeviceEventNotifier::ConvertIdStringToValue("", &id));
  EXPECT_FALSE(UsbDeviceEventNotifier::ConvertIdStringToValue("0", &id));
  EXPECT_FALSE(UsbDeviceEventNotifier::ConvertIdStringToValue("00", &id));
  EXPECT_FALSE(UsbDeviceEventNotifier::ConvertIdStringToValue("000", &id));
  EXPECT_FALSE(UsbDeviceEventNotifier::ConvertIdStringToValue("00000", &id));
  EXPECT_FALSE(UsbDeviceEventNotifier::ConvertIdStringToValue("000z", &id));

  EXPECT_TRUE(UsbDeviceEventNotifier::ConvertIdStringToValue("abcd", &id));
  EXPECT_EQ(0xabcd, id);

  EXPECT_TRUE(UsbDeviceEventNotifier::ConvertIdStringToValue("0000", &id));
  EXPECT_EQ(0x0000, id);

  EXPECT_TRUE(UsbDeviceEventNotifier::ConvertIdStringToValue("ffff", &id));
  EXPECT_EQ(0xffff, id);
}

TEST_F(UsbDeviceEventNotifierTest, OnUsbDeviceEvents) {
  MockUdevMonitor* monitor = new MockUdevMonitor();
  // Ownership of |monitor| is transferred.
  notifier_.udev_monitor_.reset(monitor);

  MockUdevDevice* device1 = new MockUdevDevice();
  MockUdevDevice* device2 = new MockUdevDevice();
  MockUdevDevice* device3 = new MockUdevDevice();
  MockUdevDevice* device4 = new MockUdevDevice();
  // Ownership of |device*| is transferred.
  EXPECT_CALL(*monitor, ReceiveDevice())
      .WillOnce(Return(device1))
      .WillOnce(Return(device2))
      .WillOnce(Return(device3))
      .WillOnce(Return(device4));

  EXPECT_CALL(*device1, GetSysPath()).WillOnce(Return(kFakeUsbDevice1SysPath));
  EXPECT_CALL(*device1, GetAction()).WillOnce(Return(kUdevActionAdd));

  EXPECT_CALL(*device2, GetSysPath()).WillOnce(Return(kFakeUsbDevice2SysPath));
  EXPECT_CALL(*device2, GetAction()).WillOnce(Return(kUdevActionAdd));
  EXPECT_CALL(*device2, GetSysAttributeValue(_))
      .WillOnce(Return(kFakeUsbDevice2VendorIdString))
      .WillOnce(Return(kFakeUsbDevice2ProductIdString));

  EXPECT_CALL(*device3, GetSysPath()).WillOnce(Return(kFakeUsbDevice1SysPath));
  EXPECT_CALL(*device3, GetAction()).WillOnce(Return(kUdevActionRemove));

  EXPECT_CALL(*device4, GetSysPath()).WillOnce(Return(kFakeUsbDevice2SysPath));
  EXPECT_CALL(*device4, GetAction()).WillOnce(Return(kUdevActionRemove));

  EXPECT_CALL(observer_,
              OnUsbDeviceAdded(kFakeUsbDevice2SysPath,
                               kFakeUsbDevice2VendorId,
                               kFakeUsbDevice2ProductId));
  EXPECT_CALL(observer_, OnUsbDeviceRemoved(kFakeUsbDevice1SysPath));

  notifier_.OnFileCanReadWithoutBlocking(kFakeUdevMonitorFileDescriptor);
  notifier_.AddObserver(&observer_);
  notifier_.OnFileCanReadWithoutBlocking(kFakeUdevMonitorFileDescriptor);
  notifier_.OnFileCanReadWithoutBlocking(kFakeUdevMonitorFileDescriptor);
  notifier_.RemoveObserver(&observer_);
  notifier_.OnFileCanReadWithoutBlocking(kFakeUdevMonitorFileDescriptor);
}

TEST_F(UsbDeviceEventNotifierTest, OnUsbDeviceEventNotAddOrRemove) {
  MockUdevMonitor* monitor = new MockUdevMonitor();
  // Ownership of |monitor| is transferred.
  notifier_.udev_monitor_.reset(monitor);

  MockUdevDevice* device = new MockUdevDevice();
  // Ownership of |device| is transferred.
  EXPECT_CALL(*monitor, ReceiveDevice()).WillOnce(Return(device));

  EXPECT_CALL(*device, GetSysPath()).WillOnce(Return(kFakeUsbDevice1SysPath));
  EXPECT_CALL(*device, GetAction()).WillOnce(Return(kUdevActionChange));

  EXPECT_CALL(observer_, OnUsbDeviceAdded(_, _, _)).Times(0);
  EXPECT_CALL(observer_, OnUsbDeviceRemoved(_)).Times(0);

  notifier_.AddObserver(&observer_);
  notifier_.OnFileCanReadWithoutBlocking(kFakeUdevMonitorFileDescriptor);
}

TEST_F(UsbDeviceEventNotifierTest, OnUsbDeviceEventWithInvalidVendorId) {
  MockUdevMonitor* monitor = new MockUdevMonitor();
  // Ownership of |monitor| is transferred.
  notifier_.udev_monitor_.reset(monitor);

  MockUdevDevice* device = new MockUdevDevice();
  // Ownership of |device| is transferred.
  EXPECT_CALL(*monitor, ReceiveDevice()).WillOnce(Return(device));

  EXPECT_CALL(*device, GetSysPath()).WillOnce(Return(kFakeUsbDevice1SysPath));
  EXPECT_CALL(*device, GetAction()).WillOnce(Return(kUdevActionAdd));
  EXPECT_CALL(*device, GetSysAttributeValue(_)).WillOnce(Return(""));

  EXPECT_CALL(observer_, OnUsbDeviceAdded(_, _, _)).Times(0);
  EXPECT_CALL(observer_, OnUsbDeviceRemoved(_)).Times(0);

  notifier_.AddObserver(&observer_);
  notifier_.OnFileCanReadWithoutBlocking(kFakeUdevMonitorFileDescriptor);
}

TEST_F(UsbDeviceEventNotifierTest, OnUsbDeviceEventWithInvalidProductId) {
  MockUdevMonitor* monitor = new MockUdevMonitor();
  // Ownership of |monitor| is transferred.
  notifier_.udev_monitor_.reset(monitor);

  MockUdevDevice* device = new MockUdevDevice();
  // Ownership of |device| is transferred.
  EXPECT_CALL(*monitor, ReceiveDevice()).WillOnce(Return(device));

  EXPECT_CALL(*device, GetSysPath()).WillOnce(Return(kFakeUsbDevice1SysPath));
  EXPECT_CALL(*device, GetAction()).WillOnce(Return(kUdevActionAdd));
  EXPECT_CALL(*device, GetSysAttributeValue(_))
      .WillOnce(Return(kFakeUsbDevice1VendorIdString))
      .WillOnce(Return(""));

  EXPECT_CALL(observer_, OnUsbDeviceAdded(_, _, _)).Times(0);
  EXPECT_CALL(observer_, OnUsbDeviceRemoved(_)).Times(0);

  notifier_.AddObserver(&observer_);
  notifier_.OnFileCanReadWithoutBlocking(kFakeUdevMonitorFileDescriptor);
}

}  // namespace mist
