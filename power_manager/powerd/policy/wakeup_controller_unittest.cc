// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/policy/wakeup_controller.h"

#include <cstdarg>

#include "gtest/gtest.h"
#include "power_manager/common/fake_prefs.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/powerd/policy/backlight_controller_stub.h"
#include "power_manager/powerd/system/acpi_wakeup_helper_stub.h"
#include "power_manager/powerd/system/ec_wakeup_helper_stub.h"
#include "power_manager/powerd/system/udev_stub.h"

namespace power_manager {
namespace policy {

namespace {
// An artificial syspath for tests.
const char kSyspath0[] = "/sys/devices/test/0";
};

class WakeupControllerTest : public ::testing::Test {
 public:
  WakeupControllerTest() = default;
  ~WakeupControllerTest() override = default;

 protected:
  std::string GetSysattr(const std::string& syspath,
                         const std::string& sysattr) {
    std::string value;
    if (!udev_.GetSysattr(syspath, sysattr, &value))
      return "(error)";
    return value;
  }

  bool GetAcpiWakeup(const std::string& acpi_name) {
    bool value = false;
    if (!acpi_wakeup_helper_.GetWakeupEnabled(acpi_name, &value)) {
      ADD_FAILURE() << "Expected ACPI wakeup for " << acpi_name
                    << " to be defined";
    }
    return value;
  }

  // Adds a device at |syspath| with one or more udev tags. The tag list must be
  // null-terminated.
  void AddDeviceWithTags(const std::string& syspath,
                         const char* tag,
                         ...) {
    udev_.SetSysattr(syspath, WakeupController::kPowerWakeup,
                     WakeupController::kDisabled);

    std::string tags;
    va_list arg_list;
    va_start(arg_list, tag);
    while (tag) {
      if (!tags.empty())
        tags += " ";
      tags += tag;
      tag = va_arg(arg_list, const char*);
    }
    va_end(arg_list);

    udev_.TaggedDeviceChanged(syspath, tags);
  }

  void InitWakeupController() {
    prefs_.SetInt64(kAllowDockedModePref, default_allow_docked_mode_);
    wakeup_controller_.Init(&backlight_controller_, &udev_,
                            &acpi_wakeup_helper_, &ec_wakeup_helper_,
                            initial_lid_state_, initial_tablet_mode_,
                            initial_display_mode_, &prefs_);
  }

  policy::BacklightControllerStub backlight_controller_;
  system::UdevStub udev_;
  system::AcpiWakeupHelperStub acpi_wakeup_helper_;
  system::EcWakeupHelperStub ec_wakeup_helper_;
  FakePrefs prefs_;

  bool default_allow_docked_mode_ = true;

  LidState initial_lid_state_ = LidState::OPEN;
  TabletMode initial_tablet_mode_ = TabletMode::OFF;
  DisplayMode initial_display_mode_ = DisplayMode::NORMAL;

  WakeupController wakeup_controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WakeupControllerTest);
};

TEST_F(WakeupControllerTest, ConfigureWakeupOnInit) {
  AddDeviceWithTags(kSyspath0, WakeupController::kTagWakeup, nullptr);

  EXPECT_EQ(WakeupController::kDisabled,
            GetSysattr(kSyspath0, WakeupController::kPowerWakeup));
  InitWakeupController();
  EXPECT_EQ(WakeupController::kEnabled,
            GetSysattr(kSyspath0, WakeupController::kPowerWakeup));
  EXPECT_TRUE(GetAcpiWakeup(WakeupController::kTPAD));
  EXPECT_FALSE(GetAcpiWakeup(WakeupController::kTSCR));
}

TEST_F(WakeupControllerTest, ConfigureWakeupOnAdd) {
  InitWakeupController();

  // The device starts out with wakeup disabled, but should get configured by
  // WakeupController right away.
  AddDeviceWithTags(kSyspath0, WakeupController::kTagWakeup, nullptr);
  EXPECT_EQ(WakeupController::kEnabled,
            GetSysattr(kSyspath0, WakeupController::kPowerWakeup));
}

TEST_F(WakeupControllerTest, DisableWakeupWhenClosed) {
  AddDeviceWithTags(kSyspath0, WakeupController::kTagWakeup,
                    WakeupController::kTagWakeupOnlyWhenUsable,
                    WakeupController::kTagUsableWhenLaptop, nullptr);
  InitWakeupController();

  // In laptop mode, wakeup should be enabled.
  EXPECT_EQ(WakeupController::kEnabled,
            GetSysattr(kSyspath0, WakeupController::kPowerWakeup));
  EXPECT_TRUE(GetAcpiWakeup("TPAD"));

  // When the lid is closed, wakeup should be disabled.
  wakeup_controller_.SetLidState(LidState::CLOSED);
  EXPECT_EQ(WakeupController::kDisabled,
            GetSysattr(kSyspath0, WakeupController::kPowerWakeup));
  EXPECT_FALSE(GetAcpiWakeup(WakeupController::kTPAD));
}

TEST_F(WakeupControllerTest, PermanentlyDisableWakeup) {
  AddDeviceWithTags(kSyspath0, WakeupController::kTagWakeup,
                    WakeupController::kTagWakeupDisabled, nullptr);

  // Simulate a device that has wakeup enabled initially.
  udev_.SetSysattr(kSyspath0, WakeupController::kPowerWakeup,
                   WakeupController::kEnabled);
  InitWakeupController();
  EXPECT_EQ(WakeupController::kDisabled,
            GetSysattr(kSyspath0, WakeupController::kPowerWakeup));
}

TEST_F(WakeupControllerTest, ConfigureInhibit) {
  AddDeviceWithTags(kSyspath0, WakeupController::kTagInhibit,
                    WakeupController::kTagUsableWhenLaptop, nullptr);
  InitWakeupController();

  // In laptop mode, inhibit should be off.
  EXPECT_EQ("0", GetSysattr(kSyspath0, WakeupController::kInhibited));

  // When the lid is closed, inhibit should be on.
  wakeup_controller_.SetLidState(LidState::CLOSED);
  EXPECT_EQ("1", GetSysattr(kSyspath0, WakeupController::kInhibited));

  // When the lid is open, inhibit should be off again.
  wakeup_controller_.SetLidState(LidState::OPEN);
  EXPECT_EQ("0", GetSysattr(kSyspath0, WakeupController::kInhibited));
}

TEST_F(WakeupControllerTest, InhibitDocking) {
  AddDeviceWithTags(kSyspath0, WakeupController::kTagInhibit,
                    WakeupController::kTagUsableWhenLaptop,
                    WakeupController::kTagUsableWhenDocked, nullptr);
  initial_display_mode_ = DisplayMode::PRESENTATION;
  InitWakeupController();

  // In laptop mode, inhibit should be off.
  EXPECT_EQ("0", GetSysattr(kSyspath0, WakeupController::kInhibited));

  // When the lid is closed, inhibit should remain off.
  wakeup_controller_.SetLidState(LidState::CLOSED);
  EXPECT_EQ("0", GetSysattr(kSyspath0, WakeupController::kInhibited));

  // When the lid is open, inhibit should still be off.
  wakeup_controller_.SetLidState(LidState::OPEN);
  EXPECT_EQ("0", GetSysattr(kSyspath0, WakeupController::kInhibited));
}

TEST_F(WakeupControllerTest, InhibitDockingDisallowed) {
  AddDeviceWithTags(kSyspath0, WakeupController::kTagInhibit,
                    WakeupController::kTagUsableWhenLaptop,
                    WakeupController::kTagUsableWhenDocked, nullptr);
  default_allow_docked_mode_ = false;
  initial_display_mode_ = DisplayMode::PRESENTATION;
  InitWakeupController();

  // In laptop mode, inhibit should be off.
  EXPECT_EQ("0", GetSysattr(kSyspath0, WakeupController::kInhibited));

  // When the lid is closed, inhibit should be on since docking is disallowed,
  // even though the device supports it.
  wakeup_controller_.SetLidState(LidState::CLOSED);
  EXPECT_EQ("1", GetSysattr(kSyspath0, WakeupController::kInhibited));

  // When the lid is open, inhibit should be off again.
  wakeup_controller_.SetLidState(LidState::OPEN);
  EXPECT_EQ("0", GetSysattr(kSyspath0, WakeupController::kInhibited));
}

TEST_F(WakeupControllerTest, SetDisplayModeExternalInput) {
  AddDeviceWithTags(kSyspath0, WakeupController::kTagInhibit,
                    WakeupController::kTagUsableWhenLaptop,
                    WakeupController::kTagUsableWhenDocked, nullptr);
  initial_lid_state_ = LidState::CLOSED;
  InitWakeupController();

  // When the lid is closed with no external display, external input devices
  // should be inhibited.
  EXPECT_EQ("1", GetSysattr(kSyspath0, WakeupController::kInhibited));

  // When an external display is attached, device should be un-inhibited.
  wakeup_controller_.SetDisplayMode(DisplayMode::PRESENTATION);
  EXPECT_EQ("0", GetSysattr(kSyspath0, WakeupController::kInhibited));

  // When external display goes away, input should be inhibited again.
  wakeup_controller_.SetDisplayMode(DisplayMode::NORMAL);
  EXPECT_EQ("1", GetSysattr(kSyspath0, WakeupController::kInhibited));
}

TEST_F(WakeupControllerTest, SetDisplayModeInternalInput) {
  AddDeviceWithTags(kSyspath0, WakeupController::kTagInhibit,
                    WakeupController::kTagUsableWhenLaptop, nullptr);
  InitWakeupController();

  // Devices that are only usable when in laptop mode should not be inhibited
  // while the lid is open.
  EXPECT_EQ("0", GetSysattr(kSyspath0, WakeupController::kInhibited));

  // When an external display is attached, device should remain uninhibited.
  wakeup_controller_.SetDisplayMode(DisplayMode::PRESENTATION);
  EXPECT_EQ("0", GetSysattr(kSyspath0, WakeupController::kInhibited));

  // When the lid is closed, internal input should be inhibited regardless
  // of display mode.
  wakeup_controller_.SetLidState(LidState::CLOSED);
  EXPECT_EQ("1", GetSysattr(kSyspath0, WakeupController::kInhibited));

  wakeup_controller_.SetDisplayMode(DisplayMode::NORMAL);
  EXPECT_EQ("1", GetSysattr(kSyspath0, WakeupController::kInhibited));
}

TEST_F(WakeupControllerTest, AllowEcWakeupAsTabletWhenDisplayOff) {
  InitWakeupController();

  // Start in presentation mode at full brightness.
  wakeup_controller_.SetDisplayMode(DisplayMode::PRESENTATION);
  backlight_controller_.NotifyObservers(100.0,
      policy::BacklightController::BrightnessChangeCause::USER_INITIATED);

  // EC wakeups should be inhibited in tablet mode while backlight is on.
  EXPECT_FALSE(ec_wakeup_helper_.IsWakeupAsTabletAllowed());

  // Automated display off should not trigger a mode change.
  backlight_controller_.NotifyObservers(0.0,
      policy::BacklightController::BrightnessChangeCause::AUTOMATED);
  EXPECT_FALSE(ec_wakeup_helper_.IsWakeupAsTabletAllowed());

  // ...but manual should.
  backlight_controller_.NotifyObservers(0.0,
      policy::BacklightController::BrightnessChangeCause::USER_INITIATED);
  EXPECT_TRUE(ec_wakeup_helper_.IsWakeupAsTabletAllowed());

  // Leaving presentation mode should disallow it.
  wakeup_controller_.SetDisplayMode(DisplayMode::NORMAL);
  EXPECT_FALSE(ec_wakeup_helper_.IsWakeupAsTabletAllowed());
  wakeup_controller_.SetDisplayMode(DisplayMode::PRESENTATION);
  EXPECT_TRUE(ec_wakeup_helper_.IsWakeupAsTabletAllowed());

  // As should raising the brightness, even if automatic.
  backlight_controller_.NotifyObservers(10.0,
      policy::BacklightController::BrightnessChangeCause::AUTOMATED);
  EXPECT_FALSE(ec_wakeup_helper_.IsWakeupAsTabletAllowed());
}

TEST_F(WakeupControllerTest, HandleTabletMode) {
  const char kKeyboardSyspath[] = "/sys/devices/keyboard/0";
  const char kTouchscreenSyspath[] = "/sys/devices/touchscreen/0";
  AddDeviceWithTags(kKeyboardSyspath, WakeupController::kTagInhibit,
                    WakeupController::kTagWakeup,
                    WakeupController::kTagWakeupOnlyWhenUsable,
                    WakeupController::kTagUsableWhenLaptop,
                    WakeupController::kTagUsableWhenDisplayOff, nullptr);
  AddDeviceWithTags(kTouchscreenSyspath, WakeupController::kTagInhibit,
                    WakeupController::kTagWakeup,
                    WakeupController::kTagWakeupOnlyWhenUsable,
                    WakeupController::kTagUsableWhenLaptop,
                    WakeupController::kTagUsableWhenTablet, nullptr);
  initial_tablet_mode_ = TabletMode::ON;

  // While in tablet mode, the keyboard should be inhibited with wakeup
  // disabled.
  InitWakeupController();
  EXPECT_EQ("1", GetSysattr(kKeyboardSyspath, WakeupController::kInhibited));
  EXPECT_EQ(WakeupController::kDisabled,
            GetSysattr(kKeyboardSyspath, WakeupController::kPowerWakeup));
  EXPECT_EQ("0", GetSysattr(kTouchscreenSyspath, WakeupController::kInhibited));
  EXPECT_EQ(WakeupController::kEnabled,
            GetSysattr(kTouchscreenSyspath, WakeupController::kPowerWakeup));

  // Switching to laptop mode should uninhibit the keyboard and permit wakeups.
  wakeup_controller_.SetTabletMode(TabletMode::OFF);
  EXPECT_EQ("0", GetSysattr(kKeyboardSyspath, WakeupController::kInhibited));
  EXPECT_EQ(WakeupController::kEnabled,
            GetSysattr(kKeyboardSyspath, WakeupController::kPowerWakeup));
  EXPECT_EQ("0", GetSysattr(kTouchscreenSyspath, WakeupController::kInhibited));
  EXPECT_EQ(WakeupController::kEnabled,
            GetSysattr(kTouchscreenSyspath, WakeupController::kPowerWakeup));

  // Display-off mode should take precedence over tablet mode.
  wakeup_controller_.SetDisplayMode(DisplayMode::PRESENTATION);
  wakeup_controller_.SetLidState(LidState::OPEN);
  wakeup_controller_.SetTabletMode(TabletMode::ON);
  wakeup_controller_.OnBrightnessChange(
      0.0, BacklightController::BrightnessChangeCause::USER_INITIATED,
      &backlight_controller_);
  EXPECT_EQ("0", GetSysattr(kKeyboardSyspath, WakeupController::kInhibited));
  EXPECT_EQ(WakeupController::kEnabled,
            GetSysattr(kKeyboardSyspath, WakeupController::kPowerWakeup));
  EXPECT_EQ("1", GetSysattr(kTouchscreenSyspath, WakeupController::kInhibited));
  EXPECT_EQ(WakeupController::kDisabled,
            GetSysattr(kTouchscreenSyspath, WakeupController::kPowerWakeup));
}

TEST_F(WakeupControllerTest, InitWithoutBacklightController) {
  // Init with null backlight controller shouldn't crash.
  wakeup_controller_.Init(nullptr, &udev_, &acpi_wakeup_helper_,
                          &ec_wakeup_helper_, initial_lid_state_,
                          initial_tablet_mode_, initial_display_mode_, &prefs_);
}

}  // namespace policy
}  // namespace power_manager
