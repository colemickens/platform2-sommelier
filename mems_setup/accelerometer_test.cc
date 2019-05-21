// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/files/file_util.h>

#include <gtest/gtest.h>

#include "mems_setup/configuration.h"
#include "mems_setup/delegate.h"
#include "mems_setup/iio_context.h"
#include "mems_setup/iio_device.h"
#include "mems_setup/test_helper.h"
#include "mems_setup/test_mocks.h"

using mems_setup::mocks::MockDelegate;
using mems_setup::mocks::MockIioChannel;
using mems_setup::mocks::MockIioContext;
using mems_setup::mocks::MockIioDevice;
using mems_setup::testing::SensorTestBase;

namespace mems_setup {

namespace {

static gid_t kChronosGroupId = 666;

class AccelerometerTest : public SensorTestBase {
 public:
  AccelerometerTest()
      : SensorTestBase(
            "cros-ec-accel", "iio:device1", SensorKind::ACCELEROMETER) {
    mock_delegate_->AddGroup("chronos", kChronosGroupId);
  }
};

TEST_F(AccelerometerTest, MissingVpd) {
  SetSingleSensor("base");
  ConfigureVpd({{"in_accel_x_base_calibbias", "100"}});

  EXPECT_TRUE(GetConfiguration()->Configure());

  EXPECT_TRUE(
      mock_device_->ReadNumberAttribute("in_accel_x_calibbias").has_value());
  EXPECT_EQ(
      100,
      mock_device_->ReadNumberAttribute("in_accel_x_calibbias").value_or(0));
  EXPECT_FALSE(
      mock_device_->ReadNumberAttribute("in_accel_y_calibbias").has_value());
  EXPECT_FALSE(
      mock_device_->ReadNumberAttribute("in_accel_z_calibbias").has_value());
}

TEST_F(AccelerometerTest, NotNumericVpd) {
  SetSingleSensor("base");
  ConfigureVpd({{"in_accel_x_base_calibbias", "blah"},
                {"in_accel_y_base_calibbias", "104"}});

  EXPECT_TRUE(GetConfiguration()->Configure());

  EXPECT_FALSE(
      mock_device_->ReadNumberAttribute("in_accel_x_calibbias").has_value());
  EXPECT_TRUE(
      mock_device_->ReadNumberAttribute("in_accel_y_calibbias").has_value());
  EXPECT_EQ(104,
            mock_device_->ReadNumberAttribute("in_accel_y_calibbias").value());
  EXPECT_FALSE(
      mock_device_->ReadNumberAttribute("in_accel_z_calibbias").has_value());
}

TEST_F(AccelerometerTest, VpdOutOfRange) {
  SetSingleSensor("base");
  ConfigureVpd({{"in_accel_x_base_calibbias", "123456789"},
                {"in_accel_y_base_calibbias", "104"},
                {"in_accel_z_base_calibbias", "85"}});

  EXPECT_TRUE(GetConfiguration()->Configure());

  EXPECT_FALSE(
      mock_device_->ReadNumberAttribute("in_accel_x_calibbias").has_value());
  EXPECT_TRUE(
      mock_device_->ReadNumberAttribute("in_accel_y_calibbias").has_value());
  EXPECT_TRUE(
      mock_device_->ReadNumberAttribute("in_accel_z_calibbias").has_value());

  EXPECT_EQ(104,
            mock_device_->ReadNumberAttribute("in_accel_y_calibbias").value());
  EXPECT_EQ(85,
            mock_device_->ReadNumberAttribute("in_accel_z_calibbias").value());
}

TEST_F(AccelerometerTest, NotLoadingTriggerModule) {
  SetSingleSensor("base");
  ConfigureVpd({{"in_accel_x_base_calibbias", "50"},
                {"in_accel_y_base_calibbias", "104"},
                {"in_accel_z_base_calibbias", "85"}});

  EXPECT_TRUE(GetConfiguration()->Configure());

  EXPECT_EQ(0, mock_delegate_->GetNumModulesProbed());
}

TEST_F(AccelerometerTest, MultipleSensorDevice) {
  ConfigureVpd({{"in_accel_x_base_calibbias", "50"},
                {"in_accel_y_base_calibbias", "104"},
                {"in_accel_z_base_calibbias", "85"},
                {"in_accel_y_lid_calibbias", "27"}});

  EXPECT_TRUE(GetConfiguration()->Configure());

  EXPECT_EQ(
      50,
      mock_device_->ReadNumberAttribute("in_accel_x_base_calibbias").value());
  EXPECT_EQ(
      104,
      mock_device_->ReadNumberAttribute("in_accel_y_base_calibbias").value());
  EXPECT_EQ(
      85,
      mock_device_->ReadNumberAttribute("in_accel_z_base_calibbias").value());

  EXPECT_FALSE(mock_device_->ReadNumberAttribute("in_accel_x_lid_calibbias")
                   .has_value());
  EXPECT_EQ(
      27,
      mock_device_->ReadNumberAttribute("in_accel_y_lid_calibbias").value());
  EXPECT_FALSE(mock_device_->ReadNumberAttribute("in_accel_z_lid_calibbias")
                   .has_value());
}

TEST_F(AccelerometerTest, TriggerPermissions) {
  SetSingleSensor("lid");
  EXPECT_TRUE(GetConfiguration()->Configure());

  base::FilePath trigger_now = mock_trigger0_->GetPath().Append("trigger_now");
  EXPECT_NE(0, mock_delegate_->GetPermissions(trigger_now) &
                   base::FILE_PERMISSION_WRITE_BY_GROUP);
  gid_t gid = 0;
  mock_delegate_->GetOwnership(trigger_now, nullptr, &gid);
  EXPECT_EQ(kChronosGroupId, gid);
}

}  // namespace

}  // namespace mems_setup
