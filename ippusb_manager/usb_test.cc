// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ippusb_manager/usb.h"

#include <gtest/gtest.h>

namespace ippusb_manager {

TEST(UsbTest, GetUsbInfoValidInput) {
  uint16_t vid = 0;
  uint16_t pid = 0;
  EXPECT_EQ(true, GetUsbInfo("03f0_7c12", &vid, &pid));
  EXPECT_EQ(0x03f0, vid);
  EXPECT_EQ(0x7c12, pid);

  EXPECT_EQ(true, GetUsbInfo("03f_7c12", &vid, &pid));
  EXPECT_EQ(0x03f, vid);
  EXPECT_EQ(0x7c12, pid);

  EXPECT_EQ(true, GetUsbInfo("03f0_7c1", &vid, &pid));
  EXPECT_EQ(0x03f0, vid);
  EXPECT_EQ(0x7c1, pid);
}

TEST(UsbTest, GetUsbInfoInvalidInput) {
  uint16_t vid = 0;
  uint16_t pid = 0;
  EXPECT_EQ(false, GetUsbInfo("123g_1111", &vid, &pid));
  EXPECT_EQ(0, vid);
  EXPECT_EQ(0, pid);
}

TEST(UsbTest, GetUsbInfoEmptyInfo) {
  uint16_t vid = 0;
  uint16_t pid = 0;
  EXPECT_EQ(false, GetUsbInfo("", &vid, &pid));
  EXPECT_EQ(0, vid);
  EXPECT_EQ(0, pid);
}

TEST(UsbTest, GetUsbInfoSingleValue) {
  uint16_t vid = 0;
  uint16_t pid = 0;
  EXPECT_EQ(false, GetUsbInfo("03f0", &vid, &pid));
  EXPECT_EQ(0, vid);
  EXPECT_EQ(0, pid);
}

TEST(UsbTest, GetUsbInfoTooManyValues) {
  uint16_t vid = 0;
  uint16_t pid = 0;
  EXPECT_EQ(false, GetUsbInfo("03f0_7c12_1234", &vid, &pid));
  EXPECT_EQ(0, vid);
  EXPECT_EQ(0, pid);
}

}  // namespace ippusb_manager
