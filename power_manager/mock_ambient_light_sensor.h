// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_MOCK_AMBIENT_LIGHT_SENSOR_H_
#define POWER_MANAGER_MOCK_AMBIENT_LIGHT_SENSOR_H_
#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "power_manager/ambient_light_sensor.h"

using ::testing::_;
using ::testing::Return;

namespace power_manager {

class MockAmbientLightSensor : public AmbientLightSensor {
 public:
  MOCK_METHOD1(Init, bool(PowerPrefsInterface* prefs));
  void ExpectInit(PowerPrefsInterface* prefs, bool ret_val) {
    EXPECT_CALL(*this, Init(prefs))
        .WillOnce(Return(ret_val))
        .RetiresOnSaturation();
  }

  MOCK_METHOD1(AddObserver, void(AmbientLightSensorObserver* obs));
  void ExpectAddObserver(AmbientLightSensorObserver* obs) {
    EXPECT_CALL(*this, AddObserver(obs))
        .Times(1)
        .RetiresOnSaturation();
  }

  void ExpectAddObserver() {
    EXPECT_CALL(*this, AddObserver(_))
        .Times(1)
        .RetiresOnSaturation();
  }

  MOCK_METHOD1(RemoveObserver, void(AmbientLightSensorObserver* obs));
  void ExpectRemoveObserver(AmbientLightSensorObserver* obs) {
    EXPECT_CALL(*this, RemoveObserver(obs))
        .Times(1)
        .RetiresOnSaturation();
  }

  MOCK_METHOD1(EnableOrDisableSensor, void(PowerState state));
  void ExpectEnableOrDisableSensor(PowerState state) {
    EXPECT_CALL(*this, EnableOrDisableSensor(state))
        .Times(1)
        .RetiresOnSaturation();
  }

  MOCK_CONST_METHOD0(GetAmbientLightPercent, double());
  void ExpectGetAmbientLightPercent(double ret_val) {
    EXPECT_CALL(*this, GetAmbientLightPercent())
        .WillOnce(Return(ret_val))
        .RetiresOnSaturation();
  }

  MOCK_CONST_METHOD0(GetAmbientLightLux, int());
  void ExpectGetAmbientLightLux(int ret_val) {
    EXPECT_CALL(*this, GetAmbientLightLux())
        .WillOnce(Return(ret_val))
        .RetiresOnSaturation();
  }
};  // class AmbientLightSensor

}  // namespace power_manager
#endif  // POWER_MANAGER_MOCK_AMBIENT_LIGHT_SENSOR_H_
