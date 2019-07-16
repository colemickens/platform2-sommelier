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

using mems_setup::testing::SensorTestBase;

namespace mems_setup {

namespace {

class LightTest : public SensorTestBase {
 public:
  LightTest()
      : SensorTestBase("cros-ec-light", "iio:device4", SensorKind::LIGHT) {}
};

TEST_F(LightTest, PartialVpd) {
  ConfigureVpd({{"als_cal_intercept", "100"}});

  EXPECT_TRUE(GetConfiguration()->Configure());

  EXPECT_TRUE(mock_device_->ReadDoubleAttribute("in_illuminance_calibbias")
                  .has_value());
  EXPECT_EQ(100, mock_device_->ReadDoubleAttribute("in_illuminance_calibbias")
                     .value_or(0));
  EXPECT_FALSE(mock_device_->ReadDoubleAttribute("in_illuminance_calibscale")
                   .has_value());
}

TEST_F(LightTest, VpdFormatError) {
  ConfigureVpd({{"als_cal_slope", "abc"}});

  EXPECT_TRUE(GetConfiguration()->Configure());

  EXPECT_FALSE(mock_device_->ReadDoubleAttribute("in_illuminance_calibbias")
                   .has_value());
  EXPECT_FALSE(mock_device_->ReadDoubleAttribute("in_illuminance_calibscale")
                   .has_value());
}

TEST_F(LightTest, ValidVpd) {
  ConfigureVpd({{"als_cal_intercept", "1.25"}, {"als_cal_slope", "12.5"}});

  EXPECT_TRUE(GetConfiguration()->Configure());

  EXPECT_TRUE(mock_device_->ReadDoubleAttribute("in_illuminance_calibbias")
                  .has_value());
  EXPECT_EQ(1.25, mock_device_->ReadDoubleAttribute("in_illuminance_calibbias")
                     .value_or(0));
  EXPECT_TRUE(mock_device_->ReadDoubleAttribute("in_illuminance_calibscale")
                  .has_value());
  EXPECT_EQ(12.5, mock_device_->ReadDoubleAttribute("in_illuminance_calibscale")
                    .value_or(0));
}

}  // namespace

}  // namespace mems_setup
