// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include <string.h>

#include <utility>

#include <base/memory/ptr_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <brillo/test_helpers.h>
#include <gtest/gtest.h>

#include "midis/udev_handler_mock.h"

using ::testing::_;
using ::testing::Return;

namespace midis {

namespace {

const char kFakeName1[] = "Sample MIDI Device - 1";
const char kFakeSysNum1[] = "2";
const char kFakeDname1[] = "midiC2D0";
const int kFakeDevNum1 = 0;
const int kFakeSubdevs1 = 1;
const int kFakeFlags1 = 7;

const char kFakeName2[] = "Sample MIDI Device - 2";
const char kFakeSysNum2[] = "3";
const char kFakeDname2[] = "midiC3D1";
const int kFakeDevNum2 = 1;
const int kFakeSubdevs2 = 2;
const int kFakeFlags2 = 6;

}  // namespace

MATCHER_P2(DeviceMatcher, id, name, "") {
  return (id == DeviceTracker::GenerateDeviceId(arg->GetCard(),
                                                arg->GetDeviceNum()) &&
          base::EqualsCaseInsensitiveASCII(arg->GetName(), name));
}

class DeviceTrackerTest : public ::testing::Test {
 public:
  DeviceTrackerTest() : udev_handler_mock_(new UdevHandlerMock()) {}

 protected:
  virtual void SetUp() {
    info1_ = base::MakeUnique<snd_rawmidi_info>();
    memset(info1_.get(), 0, sizeof(snd_rawmidi_info));
    strncpy(
        reinterpret_cast<char*>(info1_->name), kFakeName1, sizeof(kFakeName1));
    base::StringToUint(kFakeSysNum1,
                       reinterpret_cast<unsigned*>(&info1_->card));
    info1_->device = kFakeDevNum1;
    info1_->subdevices_count = kFakeSubdevs1;
    info1_->flags = kFakeFlags1;

    info2_ = base::MakeUnique<snd_rawmidi_info>();
    memset(info2_.get(), 0, sizeof(snd_rawmidi_info));
    strncpy(
        reinterpret_cast<char*>(info2_->name), kFakeName2, sizeof(kFakeName2));
    base::StringToUint(kFakeSysNum2,
                       reinterpret_cast<unsigned*>(&info2_->card));
    info2_->device = kFakeDevNum2;
    info2_->subdevices_count = kFakeSubdevs2;
    info2_->flags = kFakeFlags2;
  }

  DeviceTracker tracker_;
  std::unique_ptr<UdevHandlerMock> udev_handler_mock_;
  std::unique_ptr<snd_rawmidi_info> info1_;
  std::unique_ptr<snd_rawmidi_info> info2_;
};

// Check whether a 2 devices get successfully added to the devices map.
TEST_F(DeviceTrackerTest, Add2DevicesPositive) {
  EXPECT_CALL(*udev_handler_mock_, InitUdevHandler())
      .WillRepeatedly(Return(true));

  EXPECT_CALL(*udev_handler_mock_, GetMidiDeviceDname(_))
      .WillOnce(Return(kFakeDname1))
      .WillOnce(Return(kFakeDname2));
  EXPECT_CALL(*udev_handler_mock_, GetDeviceInfoMock(_))
      .WillOnce(Return(info1_.release()))
      .WillOnce(Return(info2_.release()));
  EXPECT_CALL(*udev_handler_mock_, GetDeviceDevNum(_))
      .WillOnce(Return(kFakeDevNum1))
      .WillOnce(Return(kFakeDevNum2));
  EXPECT_CALL(*udev_handler_mock_, GetDeviceSysNum(_))
      .WillOnce(Return(kFakeSysNum1))
      .WillOnce(Return(kFakeSysNum2));

  tracker_.udev_handler_ = std::move(udev_handler_mock_);

  tracker_.AddDevice(nullptr);
  tracker_.AddDevice(nullptr);
  EXPECT_EQ(2, tracker_.devices_.size());

  auto it = tracker_.devices_.begin();
  uint32_t device_id = it->first;
  Device const* device = it->second.get();
  EXPECT_THAT(device, DeviceMatcher(device_id, kFakeName1));

  it++;
  device_id = it->first;
  device = it->second.get();
  EXPECT_THAT(device, DeviceMatcher(device_id, kFakeName2));
}

// Check whether a device gets successfully added, then removed from the devices
// map.
TEST_F(DeviceTrackerTest, AddRemoveDevicePositive) {
  EXPECT_CALL(*udev_handler_mock_, InitUdevHandler())
      .WillRepeatedly(Return(true));

  EXPECT_CALL(*udev_handler_mock_, GetMidiDeviceDname(_))
      .WillOnce(Return(kFakeDname1));
  EXPECT_CALL(*udev_handler_mock_, GetDeviceInfoMock(_))
      .WillOnce(Return(info1_.release()));
  EXPECT_CALL(*udev_handler_mock_, GetDeviceDevNum(_))
      .WillOnce(Return(kFakeDevNum1))
      .WillOnce(Return(kFakeDevNum1));
  EXPECT_CALL(*udev_handler_mock_, GetDeviceSysNum(_))
      .WillOnce(Return(kFakeSysNum1))
      .WillOnce(Return(kFakeSysNum1));

  tracker_.udev_handler_ = std::move(udev_handler_mock_);

  tracker_.AddDevice(nullptr);
  EXPECT_EQ(1, tracker_.devices_.size());

  tracker_.RemoveDevice(nullptr);
  EXPECT_EQ(0, tracker_.devices_.size());
}

// Check whether a device gets successfully added, but not removed.
TEST_F(DeviceTrackerTest, AddDeviceRemoveNegative) {
  EXPECT_CALL(*udev_handler_mock_, InitUdevHandler())
      .WillRepeatedly(Return(true));

  EXPECT_CALL(*udev_handler_mock_, GetMidiDeviceDname(_))
      .WillOnce(Return(kFakeDname1));
  EXPECT_CALL(*udev_handler_mock_, GetDeviceInfoMock(_))
      .WillOnce(Return(info1_.release()));
  EXPECT_CALL(*udev_handler_mock_, GetDeviceDevNum(_))
      .WillOnce(Return(kFakeDevNum1))
      .WillOnce(Return(kFakeDevNum2));
  EXPECT_CALL(*udev_handler_mock_, GetDeviceSysNum(_))
      .WillOnce(Return(kFakeSysNum1))
      .WillOnce(Return(kFakeSysNum1));

  tracker_.udev_handler_ = std::move(udev_handler_mock_);

  tracker_.AddDevice(nullptr);
  EXPECT_EQ(1, tracker_.devices_.size());

  tracker_.RemoveDevice(nullptr);
  EXPECT_EQ(1, tracker_.devices_.size());
}

}  // namespace midis

int main(int argc, char** argv) {
  SetUpTests(&argc, argv, true);
  return RUN_ALL_TESTS();
}
