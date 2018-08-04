// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/policy/cellular_controller.h"

#include <base/macros.h>
#include <gtest/gtest.h>

#include "power_manager/common/fake_prefs.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/powerd/system/udev_stub.h"

namespace power_manager {
namespace policy {
namespace {

// Stub implementation of CellularController::Delegate for use by tests.
class TestCellularControllerDelegate : public CellularController::Delegate {
 public:
  TestCellularControllerDelegate() = default;
  ~TestCellularControllerDelegate() override = default;

  int num_set_calls() const { return num_set_calls_; }
  RadioTransmitPower last_transmit_power() const {
    return last_transmit_power_;
  }

  // Resets stat members.
  void ResetStats() {
    num_set_calls_ = 0;
    last_transmit_power_ = RadioTransmitPower::UNSPECIFIED;
  }

  // CellularController::Delegate:
  void SetCellularTransmitPower(RadioTransmitPower power) override {
    CHECK_NE(power, RadioTransmitPower::UNSPECIFIED);
    num_set_calls_++;
    last_transmit_power_ = power;
  }

 private:
  // Number of times that SetCellularTransmitPower() has been called.
  int num_set_calls_ = 0;

  // Last power mode passed to SetCellularTransmitPower().
  RadioTransmitPower last_transmit_power_ = RadioTransmitPower::UNSPECIFIED;

  DISALLOW_COPY_AND_ASSIGN(TestCellularControllerDelegate);
};

}  // namespace

class CellularControllerTest : public ::testing::Test {
 public:
  CellularControllerTest() = default;
  ~CellularControllerTest() override = default;

 protected:
  // Calls |controller_|'s Init() method.
  void Init(bool enable_proximity) {
    prefs_.SetInt64(kSetCellularTransmitPowerForProximityPref,
                    enable_proximity);
    controller_.Init(&delegate_, &prefs_);
  }

  FakePrefs prefs_;
  TestCellularControllerDelegate delegate_;
  CellularController controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CellularControllerTest);
};

TEST_F(CellularControllerTest, LowPowerOnSensorDetect) {
  Init(true);
  controller_.ProximitySensorDetected(UserProximity::NEAR);
  EXPECT_EQ(1, delegate_.num_set_calls());
  EXPECT_EQ(RadioTransmitPower::LOW, delegate_.last_transmit_power());
}

TEST_F(CellularControllerTest, PowerChangeOnProximityChange) {
  Init(true);
  controller_.ProximitySensorDetected(UserProximity::NEAR);
  EXPECT_EQ(RadioTransmitPower::LOW, delegate_.last_transmit_power());

  controller_.HandleProximityChange(UserProximity::FAR);
  EXPECT_EQ(RadioTransmitPower::HIGH, delegate_.last_transmit_power());

  controller_.HandleProximityChange(UserProximity::NEAR);
  EXPECT_EQ(RadioTransmitPower::LOW, delegate_.last_transmit_power());
}

TEST_F(CellularControllerTest, SettingHonoredWhenOff) {
  Init(false);
  controller_.ProximitySensorDetected(UserProximity::NEAR);
  EXPECT_EQ(0, delegate_.num_set_calls());

  controller_.HandleProximityChange(UserProximity::FAR);
  EXPECT_EQ(0, delegate_.num_set_calls());
}

}  // namespace policy
}  // namespace power_manager
