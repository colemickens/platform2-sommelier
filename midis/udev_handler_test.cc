// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include <string>
#include <utility>

#include <base/memory/ptr_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <brillo/test_helpers.h>
#include <gtest/gtest.h>

#include "midis/test_helper.h"
#include "midis/udev_handler_mock.h"

using ::testing::_;
using ::testing::Return;

namespace midis {

namespace {

const char kFakeName1[] = "Sample MIDI Device - 1";
const uint32_t kFakeSysNum1 = 2;
const char kFakeDname1[] = "midiC2D0";
const uint32_t kFakeDevNum1 = 0;
const uint32_t kFakeSubdevs1 = 1;
const uint32_t kFakeFlags1 = 7;

const char kFakeName2[] = "Sample MIDI Device - 2";
const uint32_t kFakeSysNum2 = 3;
const char kFakeDname2[] = "midiC3D1";
const uint32_t kFakeDevNum2 = 1;
const uint32_t kFakeSubdevs2 = 2;
const uint32_t kFakeFlags2 = 6;

const char kBlankDname[] = "";

}  // namespace

class UdevHandlerTest : public ::testing::Test {
 public:
  UdevHandlerTest() : udev_handler_mock_(new UdevHandlerMock()) {}

 protected:
  virtual void SetUp() {
    info1_ = base::MakeUnique<snd_rawmidi_info>();
    memset(info1_.get(), 0, sizeof(snd_rawmidi_info));
    strncpy(
        reinterpret_cast<char*>(info1_->name), kFakeName1, sizeof(kFakeName1));
    info1_->card = kFakeSysNum1;
    info1_->device = kFakeDevNum1;
    info1_->subdevices_count = kFakeSubdevs1;
    info1_->flags = kFakeFlags1;

    info2_ = base::MakeUnique<snd_rawmidi_info>();
    memset(info2_.get(), 0, sizeof(snd_rawmidi_info));
    strncpy(
        reinterpret_cast<char*>(info2_->name), kFakeName2, sizeof(kFakeName2));
    info2_->card = kFakeSysNum2;
    info2_->device = kFakeDevNum2;
    info2_->subdevices_count = kFakeSubdevs2;
    info2_->flags = kFakeFlags2;
  }

  std::unique_ptr<UdevHandlerMock> udev_handler_mock_;
  std::unique_ptr<snd_rawmidi_info> info1_;
  std::unique_ptr<snd_rawmidi_info> info2_;
};

// Check whether Device gets created successfully.
TEST_F(UdevHandlerTest, CreateDevicePositive) {
  EXPECT_CALL(*udev_handler_mock_, GetMidiDeviceDnameMock(_))
      .WillOnce(Return(kFakeDname1))
      .WillOnce(Return(kFakeDname2));
  EXPECT_CALL(*udev_handler_mock_, GetDeviceInfoMock(_))
      .WillOnce(Return(info1_.release()))
      .WillOnce(Return(info2_.release()));

  // Usually, we need to get a reference to a device, but we have mocked
  // the functions that rely on the reference, so it's not necessary.
  // CreateDevice actually needs a udev_device reference, but since we
  // are mocking the internal calls that use it, we can bass a nullptr here
  auto device = udev_handler_mock_->CreateDevice(nullptr);
  uint32_t device_id =
      udev_handler_mock_->GenerateDeviceId(kFakeSysNum1, kFakeDevNum1);
  EXPECT_THAT(device, DeviceMatcher(device_id, kFakeName1));

  device = udev_handler_mock_->CreateDevice(nullptr);
  device_id = udev_handler_mock_->GenerateDeviceId(kFakeSysNum2, kFakeDevNum2);
  EXPECT_THAT(device, DeviceMatcher(device_id, kFakeName2));
}

// Check behaviour when GetMidiDeviceName and GetDeviceInfo returns nullptr
TEST_F(UdevHandlerTest, CreateDeviceNegative1) {
  EXPECT_CALL(*udev_handler_mock_, GetMidiDeviceDnameMock(_))
      .WillOnce(Return(kBlankDname))
      .WillOnce(Return(kFakeDname1));
  EXPECT_CALL(*udev_handler_mock_, GetDeviceInfoMock(_))
      .WillOnce(Return(nullptr));

  auto device = udev_handler_mock_->CreateDevice(nullptr);
  EXPECT_EQ(device.get(), nullptr);

  device = udev_handler_mock_->CreateDevice(nullptr);
  EXPECT_EQ(device.get(), nullptr);
}

}  // namespace midis

int main(int argc, char** argv) {
  SetUpTests(&argc, argv, true);
  return RUN_ALL_TESTS();
}
