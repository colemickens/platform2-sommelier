// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include <libmems/iio_context.h>
#include <libmems/iio_device.h>
#include <libmems/test_fakes.h>
#include "mems_setup/configuration.h"
#include "mems_setup/delegate.h"
#include "mems_setup/sensor_location.h"
#include "mems_setup/test_fakes.h"
#include "mems_setup/test_helper.h"

using libmems::fakes::FakeIioChannel;
using libmems::fakes::FakeIioContext;
using libmems::fakes::FakeIioDevice;
using mems_setup::fakes::FakeDelegate;
using mems_setup::testing::SensorTestBase;

namespace mems_setup {

namespace {

class GyroscopeTest : public SensorTestBase {
 public:
  GyroscopeTest()
      : SensorTestBase("cros-ec-gyro", "iio:device0", SensorKind::GYROSCOPE) {}
};

TEST_F(GyroscopeTest, MissingVpd) {
  SetSingleSensor(kBaseSensorLocation);
  ConfigureVpd({{"in_anglvel_x_base_calibbias", "100"}});

  EXPECT_TRUE(GetConfiguration()->Configure());

  EXPECT_TRUE(
      mock_device_->ReadNumberAttribute("in_anglvel_x_calibbias").has_value());
  EXPECT_EQ(
      100,
      mock_device_->ReadNumberAttribute("in_anglvel_x_calibbias").value_or(0));
  EXPECT_FALSE(
      mock_device_->ReadNumberAttribute("in_anglvel_y_calibbias").has_value());
  EXPECT_FALSE(
      mock_device_->ReadNumberAttribute("in_anglvel_z_calibbias").has_value());
}

TEST_F(GyroscopeTest, NotNumericVpd) {
  SetSingleSensor(kBaseSensorLocation);
  ConfigureVpd({{"in_anglvel_x_base_calibbias", "blah"},
                {"in_anglvel_y_base_calibbias", "104"}});

  EXPECT_TRUE(GetConfiguration()->Configure());

  EXPECT_FALSE(
      mock_device_->ReadNumberAttribute("in_anglvel_x_calibbias").has_value());
  EXPECT_TRUE(
      mock_device_->ReadNumberAttribute("in_anglvel_y_calibbias").has_value());
  EXPECT_EQ(
      104, mock_device_->ReadNumberAttribute("in_anglvel_y_calibbias").value());
  EXPECT_FALSE(
      mock_device_->ReadNumberAttribute("in_anglvel_z_calibbias").has_value());
}

TEST_F(GyroscopeTest, VpdOutOfRange) {
  SetSingleSensor(kBaseSensorLocation);
  ConfigureVpd({{"in_anglvel_x_base_calibbias", "123456789"},
                {"in_anglvel_y_base_calibbias", "104"},
                {"in_anglvel_z_base_calibbias", "85"}});

  EXPECT_TRUE(GetConfiguration()->Configure());

  EXPECT_FALSE(
      mock_device_->ReadNumberAttribute("in_anglvel_x_calibbias").has_value());
  EXPECT_TRUE(
      mock_device_->ReadNumberAttribute("in_anglvel_y_calibbias").has_value());
  EXPECT_TRUE(
      mock_device_->ReadNumberAttribute("in_anglvel_z_calibbias").has_value());

  EXPECT_EQ(
      104, mock_device_->ReadNumberAttribute("in_anglvel_y_calibbias").value());
  EXPECT_EQ(
      85, mock_device_->ReadNumberAttribute("in_anglvel_z_calibbias").value());
}

TEST_F(GyroscopeTest, NotLoadingTriggerModule) {
  SetSingleSensor(kBaseSensorLocation);
  ConfigureVpd({{"in_anglvel_x_base_calibbias", "50"},
                {"in_anglvel_y_base_calibbias", "104"},
                {"in_anglvel_z_base_calibbias", "85"}});

  EXPECT_TRUE(GetConfiguration()->Configure());

  EXPECT_EQ(0, mock_delegate_->GetNumModulesProbed());
}

TEST_F(GyroscopeTest, MultipleSensorDevice) {
  ConfigureVpd({{"in_anglvel_x_base_calibbias", "50"},
                {"in_anglvel_y_base_calibbias", "104"},
                {"in_anglvel_z_base_calibbias", "85"},
                {"in_anglvel_y_lid_calibbias", "27"}});

  EXPECT_TRUE(GetConfiguration()->Configure());

  EXPECT_EQ(
      50,
      mock_device_->ReadNumberAttribute("in_anglvel_x_base_calibbias").value());
  EXPECT_EQ(
      104,
      mock_device_->ReadNumberAttribute("in_anglvel_y_base_calibbias").value());
  EXPECT_EQ(
      85,
      mock_device_->ReadNumberAttribute("in_anglvel_z_base_calibbias").value());

  EXPECT_FALSE(mock_device_->ReadNumberAttribute("in_anglvel_x_lid_calibbias")
                   .has_value());
  EXPECT_EQ(
      27,
      mock_device_->ReadNumberAttribute("in_anglvel_y_lid_calibbias").value());
  EXPECT_FALSE(mock_device_->ReadNumberAttribute("in_anglvel_z_lid_calibbias")
                   .has_value());
}

}  // namespace

}  // namespace mems_setup
