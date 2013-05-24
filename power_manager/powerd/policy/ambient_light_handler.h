// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_POLICY_AMBIENT_LIGHT_HANDLER_H_
#define POWER_MANAGER_POWERD_POLICY_AMBIENT_LIGHT_HANDLER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/powerd/system/ambient_light_observer.h"

namespace power_manager {

class PrefsInterface;

namespace system {
class AmbientLightSensorInterface;
}  // namespace system

namespace policy {

// Observes changes to ambient light reported by system::AmbientLightSensor
// and makes decisions about when backlight brightness should be adjusted.
class AmbientLightHandler : public system::AmbientLightObserver {
 public:
  enum BrightnessChangeCause {
    CAUSED_BY_AMBIENT_LIGHT = 0,
    CAUSED_BY_POWER_SOURCE,
  };

  // Interface for classes that perform actions on behalf of
  // AmbientLightHandler.
  class Delegate {
   public:
    Delegate() {}
    virtual ~Delegate() {}

    // Invoked when the backlight brightness should be adjusted in response
    // to a change in ambient light.
    virtual void SetBrightnessPercentForAmbientLight(
        double brightness_percent,
        BrightnessChangeCause cause) = 0;
  };

  AmbientLightHandler(system::AmbientLightSensorInterface* sensor,
                      Delegate* delegate);
  virtual ~AmbientLightHandler();

  double min_brightness_percent() const { return min_brightness_percent_; }
  double dimmed_brightness_percent() const {
    return dimmed_brightness_percent_;
  }

  void set_name(const std::string& name) { name_ = name; }

  // Initializes the object based on the data in the preferences
  // |limits_pref_name| and |steps_pref_name| stored within |prefs|.
  // |lux_level_| is initialized to a synthetic value based on
  // |initial_brightness_percent|, the backlight brightness at the time of
  // initialization.
  //
  // |limits_pref_name|'s value should contain three lines:
  //
  //   <min-percentage>
  //   <dimmed-percentage>
  //   <max-percentage>
  //
  // |steps_pref_name|'s value should contain one or more newline-separated
  // brightness steps, each containing three or four space-separated
  // values:
  //
  //   <ac-backlight-percentage>
  //     <battery-backlight-percentage> (optional)
  //     <decrease-lux-threshold>
  //     <increase-lux-threshold>
  //
  // These values' meanings are described in BrightnessStep.
  void Init(PrefsInterface* prefs,
            const std::string& limits_pref_name,
            const std::string& steps_pref_name,
            double initial_brightness_percent);

  // Should be called when the power source changes.
  void HandlePowerSourceChange(PowerSource source);

  // system::AmbientLightObserver implementation:
  virtual void OnAmbientLightUpdated(
      system::AmbientLightSensorInterface* sensor) OVERRIDE;

 private:
  // Contains information from prefs about a brightness step.
  struct BrightnessStep {
    // Backlight brightness in the range [0.0, 100.0] that corresponds to
    // this step.
    double ac_target_percent;
    double battery_target_percent;

    // If the lux level reported by |sensor_| drops below this value, a
    // lower step should be used.  -1 represents negative infinity.
    int decrease_lux_threshold;

    // If the lux level reported by |sensor_| increases above this value, a
    // higher step should be used.  -1 represents positive infinity.
    int increase_lux_threshold;
  };

  enum HysteresisState {
    // The most-recent lux level matched |lux_level_|.
    HYSTERESIS_STABLE,
    // The most-recent lux level was less than |lux_level_|.
    HYSTERESIS_DECREASING,
    // The most-recent lux level was greater than |lux_level_|.
    HYSTERESIS_INCREASING,
    // The brightness should be adjusted immediately after the next sensor
    // reading.
    HYSTERESIS_IMMEDIATE,
  };

  // Returns the current target backlight brightness percent based on
  // |step_index_| and |power_source_|.
  double GetTargetPercent() const;

  system::AmbientLightSensorInterface* sensor_;  // not owned
  Delegate* delegate_;  // not owned

  PowerSource power_source_;

  // Minimum, dimmed, and maximum brightness percentages that the backlight
  // should be set to.
  double min_brightness_percent_;
  double dimmed_brightness_percent_;
  double max_brightness_percent_;

  // Value from |sensor_| at the time of the last brightness adjustment.
  int lux_level_;

  HysteresisState hysteresis_state_;

  // If |hysteresis_state_| is HYSTERESIS_DECREASING or
  // HYSTERESIS_INCREASING, number of readings that have been received in
  // the current state.
  int hysteresis_count_;

  // Brightness step data read from prefs. It is assumed that this data is
  // well-formed; specifically, for each entry in the file, the decrease
  // thresholds are monotonically increasing and the increase thresholds
  // are monotonically decreasing.
  std::vector<BrightnessStep> steps_;

  // Current brightness step within |steps_|.
  size_t step_index_;

  // Has |delegate_| been notified about an ambient-light-triggered change
  // yet?
  bool sent_initial_adjustment_;

  // Human-readable name included in logging messages.  Useful for
  // distinguishing between different AmbientLightHandler instances.
  std::string name_;

  DISALLOW_COPY_AND_ASSIGN(AmbientLightHandler);
};

}  // namespace policy
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_POLICY_AMBIENT_LIGHT_HANDLER_H_
