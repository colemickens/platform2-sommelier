// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mist/usb_device_event_notifier.h"

#include <gtest/gtest.h>

namespace mist {

TEST(UsbDeviceEventNotifierTest, ConvertNullToEmptyString) {
  EXPECT_EQ("", UsbDeviceEventNotifier::ConvertNullToEmptyString(NULL));
  EXPECT_EQ("", UsbDeviceEventNotifier::ConvertNullToEmptyString(""));
  EXPECT_EQ("a", UsbDeviceEventNotifier::ConvertNullToEmptyString("a"));
  EXPECT_EQ("test string",
            UsbDeviceEventNotifier::ConvertNullToEmptyString("test string"));
}

TEST(UsbDeviceEventNotifierTest, ConvertIdStringToValue) {
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

}  // namespace mist
