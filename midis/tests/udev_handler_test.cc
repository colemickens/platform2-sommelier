// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <brillo/daemons/daemon.h>
#include <brillo/test_helpers.h>
#include <gtest/gtest.h>

#include "midis/tests/test_helper.h"
#include "midis/udev_handler_mock.h"

using ::testing::_;
using ::testing::Return;

namespace midis {

namespace {

const char kFakeName1[] = "Sample MIDI Device - 1";
const char kFakeManufacturer1[] = "Foo";
const uint32_t kFakeSysNum1 = 2;
const char kFakeDname1[] = "midiC2D0";
const uint32_t kFakeDevNum1 = 0;
const uint32_t kFakeSubdevs1 = 1;
const uint32_t kFakeFlags1 = 7;

const char kFakeName2[] = "Sample MIDI Device - 2";
const char kFakeManufacturer2[] = "Bar";
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
  void SetUp() override {
    message_loop_.SetAsCurrent();
    info1_ = std::make_unique<snd_rawmidi_info>();
    memset(info1_.get(), 0, sizeof(snd_rawmidi_info));
    strncpy(reinterpret_cast<char*>(info1_->name), kFakeName1,
            sizeof(kFakeName1));
    info1_->card = kFakeSysNum1;
    info1_->device = kFakeDevNum1;
    info1_->subdevices_count = kFakeSubdevs1;
    info1_->flags = kFakeFlags1;

    info2_ = std::make_unique<snd_rawmidi_info>();
    memset(info2_.get(), 0, sizeof(snd_rawmidi_info));
    strncpy(reinterpret_cast<char*>(info2_->name), kFakeName2,
            sizeof(kFakeName2));
    info2_->card = kFakeSysNum2;
    info2_->device = kFakeDevNum2;
    info2_->subdevices_count = kFakeSubdevs2;
    info2_->flags = kFakeFlags2;

    // Create fake dev nod files
    CreateNewTempDirectory(std::string(), &temp_fp_);
    if (temp_fp_.empty()) {
      LOG(ERROR) << "Unable to create temporary directory.";
      return;
    }

    base::FilePath dev_path = CreateFakeTempSubDir(temp_fp_, "dev/snd");
    if (dev_path.value() == "") {
      LOG(ERROR) << "Unable to fake dev/snd directory.";
      return;
    }

    base::FilePath dev_node_path =
        CreateDevNodeFileName(dev_path, kFakeSysNum1, kFakeDevNum1);
    int ret = base::WriteFile(dev_node_path, nullptr, 0);
    if (ret == -1) {
      LOG(ERROR) << "Unabled to create fake devnode.";
      return;
    }

    dev_node_path = CreateDevNodeFileName(dev_path, kFakeSysNum2, kFakeDevNum2);
    ret = base::WriteFile(dev_node_path, nullptr, 0);
    if (ret == -1) {
      LOG(ERROR) << "Unabled to create fake devnode.";
      return;
    }
  }

  void TearDown() override { base::DeleteFile(temp_fp_, true); }

  std::unique_ptr<UdevHandlerMock> udev_handler_mock_;
  std::unique_ptr<snd_rawmidi_info> info1_;
  std::unique_ptr<snd_rawmidi_info> info2_;
  base::FilePath temp_fp_;

 private:
  brillo::BaseMessageLoop message_loop_;
};

// Check whether Device gets created successfully.
TEST_F(UdevHandlerTest, CreateDevicePositive) {
  EXPECT_CALL(*udev_handler_mock_, GetMidiDeviceDnameMock(_))
      .WillOnce(Return(kFakeDname1))
      .WillOnce(Return(kFakeDname2));
  EXPECT_CALL(*udev_handler_mock_, GetDeviceInfoMock(_))
      .WillOnce(Return(info1_.release()))
      .WillOnce(Return(info2_.release()));
  EXPECT_CALL(*udev_handler_mock_, ExtractManufacturerStringMock(_, _))
      .WillOnce(Return(kFakeManufacturer1))
      .WillOnce(Return(kFakeManufacturer2));

  ASSERT_FALSE(temp_fp_.empty());

  Device::SetBaseDirForTesting(temp_fp_);
  // Usually, we need to get a reference to a device, but we have mocked
  // the functions that rely on the reference, so it's not necessary.
  // CreateDevice actually needs a udev_device reference, but since we
  // are mocking the internal calls that use it, we can bass a nullptr here
  auto device = udev_handler_mock_->CreateDevice(nullptr);
  uint32_t device_id =
      udev_handler_mock_->GenerateDeviceId(kFakeSysNum1, kFakeDevNum1);
  EXPECT_THAT(device, DeviceMatcher(device_id, kFakeName1, kFakeManufacturer1));

  device = udev_handler_mock_->CreateDevice(nullptr);
  device_id = udev_handler_mock_->GenerateDeviceId(kFakeSysNum2, kFakeDevNum2);
  EXPECT_THAT(device, DeviceMatcher(device_id, kFakeName2, kFakeManufacturer2));
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
