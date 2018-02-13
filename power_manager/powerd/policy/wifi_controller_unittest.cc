// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/policy/wifi_controller.h"

#include <base/macros.h>
#include <gtest/gtest.h>

#include "power_manager/common/fake_prefs.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/powerd/system/udev_stub.h"

namespace power_manager {
namespace policy {
namespace {

// Stub implementation of WifiController::Delegate for use by tests.
class TestWifiControllerDelegate : public WifiController::Delegate {
 public:
  TestWifiControllerDelegate() = default;
  ~TestWifiControllerDelegate() override = default;

  int num_set_calls() const { return num_set_calls_; }
  TabletMode last_tablet_mode() const { return last_tablet_mode_; }

  // Resets stat members.
  void ResetStats() {
    num_set_calls_ = 0;
    last_tablet_mode_ = TabletMode::UNSUPPORTED;
  }

  // WifiController::Delegate:
  void SetWifiTransmitPower(TabletMode tablet_mode) override {
    CHECK_NE(tablet_mode, TabletMode::UNSUPPORTED);
    num_set_calls_++;
    last_tablet_mode_ = tablet_mode;
  }

 private:
  // Number of times that SetWifiTransmitPower() has been called.
  int num_set_calls_ = 0;

  // Last tablet mode passed to SetWifiTransmitPower().
  TabletMode last_tablet_mode_ = TabletMode::UNSUPPORTED;

  DISALLOW_COPY_AND_ASSIGN(TestWifiControllerDelegate);
};

}  // namespace

class WifiControllerTest : public ::testing::Test {
 public:
  WifiControllerTest() = default;
  ~WifiControllerTest() override = default;

 protected:
  // Calls |controller_|'s Init() method.
  void Init(TabletMode tablet_mode) {
    prefs_.SetInt64(kSetWifiTransmitPowerForTabletModePref,
                    set_transmit_power_pref_value_);
    controller_.Init(&delegate_, &prefs_, &udev_, tablet_mode);
  }

  // Sends a udev event announcing that a wifi device has been added.
  void SendUdevEvent() {
    udev_.NotifySubsystemObservers(
        {{WifiController::kUdevSubsystem, WifiController::kUdevDevtype, "", ""},
         system::UdevEvent::Action::ADD});
  }

  // Initial value for kSetWifiTransmitPowerForTabletModePref.
  bool set_transmit_power_pref_value_ = true;

  system::UdevStub udev_;
  FakePrefs prefs_;
  TestWifiControllerDelegate delegate_;
  WifiController controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WifiControllerTest);
};

TEST_F(WifiControllerTest, SetTransmitPowerForInitialTabletMode) {
  Init(TabletMode::ON);
  EXPECT_EQ(1, delegate_.num_set_calls());
  EXPECT_EQ(TabletMode::ON, delegate_.last_tablet_mode());
}

TEST_F(WifiControllerTest, SetTransmitPowerForInitialClamshellMode) {
  Init(TabletMode::OFF);
  EXPECT_EQ(1, delegate_.num_set_calls());
  EXPECT_EQ(TabletMode::OFF, delegate_.last_tablet_mode());
}

TEST_F(WifiControllerTest, SetTransmitPowerForTabletModeChange) {
  Init(TabletMode::OFF);
  delegate_.ResetStats();

  controller_.HandleTabletModeChange(TabletMode::ON);
  EXPECT_EQ(1, delegate_.num_set_calls());
  EXPECT_EQ(TabletMode::ON, delegate_.last_tablet_mode());

  controller_.HandleTabletModeChange(TabletMode::OFF);
  EXPECT_EQ(2, delegate_.num_set_calls());
  EXPECT_EQ(TabletMode::OFF, delegate_.last_tablet_mode());

  // Don't set the power if the tablet mode didn't change.
  controller_.HandleTabletModeChange(TabletMode::OFF);
  EXPECT_EQ(2, delegate_.num_set_calls());
}

TEST_F(WifiControllerTest, SetTransmitPowerForDeviceAdded) {
  Init(TabletMode::ON);
  delegate_.ResetStats();

  // Attempt to set transmit power again when a wifi device is added.
  SendUdevEvent();
  EXPECT_EQ(1, delegate_.num_set_calls());
  EXPECT_EQ(TabletMode::ON, delegate_.last_tablet_mode());

  // Non-add events, or additions of non-wifi devices, shouldn't do anything.
  udev_.NotifySubsystemObservers(
      {{WifiController::kUdevSubsystem, WifiController::kUdevDevtype, "", ""},
       system::UdevEvent::Action::CHANGE});
  EXPECT_EQ(1, delegate_.num_set_calls());
  udev_.NotifySubsystemObservers(
      {{WifiController::kUdevSubsystem, "eth", "", ""},
       system::UdevEvent::Action::ADD});
  EXPECT_EQ(1, delegate_.num_set_calls());
}

TEST_F(WifiControllerTest, DontSetTransmitPowerWhenUnsupported) {
  // The delegate shouldn't be called if tablet mode is unsupported.
  Init(TabletMode::UNSUPPORTED);
  EXPECT_EQ(0, delegate_.num_set_calls());
  controller_.HandleTabletModeChange(TabletMode::UNSUPPORTED);
  EXPECT_EQ(0, delegate_.num_set_calls());
  SendUdevEvent();
  EXPECT_EQ(0, delegate_.num_set_calls());
}

TEST_F(WifiControllerTest, DontSetTransmitPowerWhenDisabled) {
  // The delegate should never be called when the pref is set to false.
  set_transmit_power_pref_value_ = false;
  Init(TabletMode::ON);
  EXPECT_EQ(0, delegate_.num_set_calls());
  controller_.HandleTabletModeChange(TabletMode::OFF);
  EXPECT_EQ(0, delegate_.num_set_calls());
  SendUdevEvent();
  EXPECT_EQ(0, delegate_.num_set_calls());
}

}  // namespace policy
}  // namespace power_manager
