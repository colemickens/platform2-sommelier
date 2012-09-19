// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_EXTERNAL_BACKLIGHT_CONTROLLER_H_
#define POWER_MANAGER_EXTERNAL_BACKLIGHT_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "power_manager/backlight_controller.h"

namespace power_manager {

class BacklightInterface;

// Controls the brightness of an external display on machines that lack internal
// displays.
class ExternalBacklightController : public BacklightController {
 public:
  explicit ExternalBacklightController(BacklightInterface* backlight);
  virtual ~ExternalBacklightController();

  bool currently_dimming() const { return currently_dimming_; }
  bool currently_off() const { return currently_off_; }

  void set_disable_dbus_for_testing(bool disable) {
    disable_dbus_for_testing_ = disable;
  }

  // BacklightController implementation:
  virtual bool Init() OVERRIDE;
  virtual void SetMonitorReconfigure(
      MonitorReconfigure* monitor_reconfigure) OVERRIDE;
  virtual void SetObserver(BacklightControllerObserver* observer) OVERRIDE;
  virtual double GetTargetBrightnessPercent() OVERRIDE;
  virtual bool GetCurrentBrightnessPercent(double* percent) OVERRIDE;
  virtual bool SetCurrentBrightnessPercent(double percent,
                                           BrightnessChangeCause cause,
                                           TransitionStyle style) OVERRIDE;
  virtual bool IncreaseBrightness(BrightnessChangeCause cause) OVERRIDE;
  virtual bool DecreaseBrightness(bool allow_off,
                                  BrightnessChangeCause cause) OVERRIDE;
  virtual bool SetPowerState(PowerState state) OVERRIDE;
  virtual PowerState GetPowerState() const OVERRIDE;
  virtual bool OnPlugEvent(bool is_plugged) OVERRIDE;
  virtual bool IsBacklightActiveOff() OVERRIDE;
  virtual int GetNumAmbientLightSensorAdjustments() const OVERRIDE;
  virtual int GetNumUserAdjustments() const OVERRIDE;

  // BacklightInterfaceObserver implementation:
  virtual void OnBacklightDeviceChanged() OVERRIDE;

  // Implementation of AmbientLightSensorObserver
  virtual void OnAmbientLightChanged(AmbientLightSensor* sensor) OVERRIDE;

 private:
  friend class ExternalBacklightControllerTest;

  double LevelToPercent(int64 level);
  int64 PercentToLevel(double percent);

  // Adjusts the brightness by |percent_offset|.
  bool AdjustBrightnessByOffset(double percent_offset,
                                BrightnessChangeCause cause);

  // Emits a D-Bus signal asking Chrome to dim or undim the screen.  |state| is
  // one of the kSoftwareScreenDimming* constants defined in
  // chromeos/dbus/service_constants.h.
  void SendSoftwareDimmingSignal(int state);

  BacklightInterface* backlight_;  // not owned
  MonitorReconfigure* monitor_reconfigure_;  // not owned
  BacklightControllerObserver* observer_;  // not owned

  PowerState power_state_;

  // Maximum brightness level exposed by the current display.
  // 0 is always the minimum.
  int64 max_level_;

  // Is Chrome currently dimming the screen on our behalf?
  bool currently_dimming_;

  // Are the external displays currently turned off?
  bool currently_off_;

  // Number of times that we've applied user-initiated brightness requests.
  int num_user_adjustments_;

  // Set by tests to disable emitting D-Bus signals.
  bool disable_dbus_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(ExternalBacklightController);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_EXTERNAL_BACKLIGHT_CONTROLLER_H_
