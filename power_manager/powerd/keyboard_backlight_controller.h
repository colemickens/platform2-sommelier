// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_KEYBOARD_BACKLIGHT_CONTROLLER_H_
#define POWER_MANAGER_POWERD_KEYBOARD_BACKLIGHT_CONTROLLER_H_

#include <glib.h>
#include <vector>

#include "base/compiler_specific.h"
#include "base/time.h"
#include "power_manager/common/signal_callback.h"
#include "power_manager/powerd/ambient_light_sensor.h"
#include "power_manager/powerd/backlight_controller.h"
#include "power_manager/powerd/video_detector.h"

namespace power_manager {

class KeyboardBacklightControllerTest;
class PrefsInterface;

namespace system {
class BacklightInterface;
}  // namespace system

// Controls the keyboard backlight for devices with such a backlight.
class KeyboardBacklightController : public BacklightController,
                                    public VideoDetectorObserver {
 public:
  KeyboardBacklightController(system::BacklightInterface* backlight,
                              PrefsInterface* prefs,
                              AmbientLightSensor* sensor);
  virtual ~KeyboardBacklightController();

  // Total durations for different transition styles, in milliseconds.
  static const int64 kFastTransitionMs;
  static const int64 kSlowTransitionMs;

  void set_current_time_for_testing(base::TimeTicks now) {
    current_time_for_testing_ = now;
  }

  // Implementation of BacklightController
  virtual bool Init() OVERRIDE;
  virtual void SetMonitorReconfigure(
      MonitorReconfigureInterface* monitor_reconfigure) OVERRIDE {}
  virtual void SetObserver(BacklightControllerObserver* observer) OVERRIDE;
  virtual double GetTargetBrightnessPercent() OVERRIDE;
  virtual bool GetCurrentBrightnessPercent(double* percent) OVERRIDE;
  virtual bool SetCurrentBrightnessPercent(double percent,
                                           BrightnessChangeCause cause,
                                           TransitionStyle style) OVERRIDE;
  virtual bool IncreaseBrightness(BrightnessChangeCause cause) OVERRIDE;
  virtual bool DecreaseBrightness(bool allow_off,
                                  BrightnessChangeCause cause) OVERRIDE;
  virtual bool SetPowerState(PowerState new_state) OVERRIDE;
  virtual PowerState GetPowerState() const OVERRIDE;
  virtual bool OnPlugEvent(bool is_plugged) OVERRIDE { return true; };
  virtual bool IsBacklightActiveOff() OVERRIDE;
  virtual int GetNumAmbientLightSensorAdjustments() const OVERRIDE;
  virtual int GetNumUserAdjustments() const OVERRIDE;

  // Implementation of BacklightInterfaceObserver
  virtual void OnBacklightDeviceChanged() OVERRIDE;

  // Implementation of AmbientLightSensorObserver
  virtual void OnAmbientLightChanged(AmbientLightSensor* sensor) OVERRIDE;

  // Implementation of VideoDetectorObserver
  virtual void OnVideoDetectorEvent(base::TimeTicks last_activity_time,
                                    bool is_fullscreen) OVERRIDE;

 private:
  friend class KeyboardBacklightControllerTest;
  FRIEND_TEST(KeyboardBacklightControllerTest, Init);
  FRIEND_TEST(KeyboardBacklightControllerTest, GetCurrentBrightnessPercent);
  FRIEND_TEST(KeyboardBacklightControllerTest, SetCurrentBrightnessPercent);
  FRIEND_TEST(KeyboardBacklightControllerTest, OnVideoDetectorEvent);
  FRIEND_TEST(KeyboardBacklightControllerTest, UpdateBacklightEnabled);
  FRIEND_TEST(KeyboardBacklightControllerTest, WriteBrightnessLevel);
  FRIEND_TEST(KeyboardBacklightControllerTest, HaltVideoTimeout);
  FRIEND_TEST(KeyboardBacklightControllerTest, VideoTimeout);
  FRIEND_TEST(KeyboardBacklightControllerTest, LevelToPercent);
  FRIEND_TEST(KeyboardBacklightControllerTest, PercentToLevel);
  FRIEND_TEST(KeyboardBacklightControllerTest, OnAmbientLightChanged);
  FRIEND_TEST(KeyboardBacklightControllerTest, Transitions);
  FRIEND_TEST(KeyboardBacklightControllerTest, ReadLimitsPrefs);
  FRIEND_TEST(KeyboardBacklightControllerTest, ReadAlsStepsPref);
  FRIEND_TEST(KeyboardBacklightControllerTest, ReadUserStepsPref);
  FRIEND_TEST(KeyboardBacklightControllerTest, InitializeUserStepIndex);
  FRIEND_TEST(KeyboardBacklightControllerTest, GetNewLevel);
  FRIEND_TEST(KeyboardBacklightControllerTest, IncreaseBrightness);
  FRIEND_TEST(KeyboardBacklightControllerTest, DecreaseBrightness);

  // Contains the information about a brightness step for ALS adjustments. The
  // data for instances of this structure are read out of a prefs
  // file. |target_percent| is the brightness level of the backlight that this
  // step causes. The thresholds are used to determine when to leave this
  // state. Specifically as the lux value is decreasing if the lux value is
  // lower then |decrease_threshold| is used. The equivalent logic is used in
  // the increasing case. The step whose threshold is closest but not passed by
  // the lux value is the one that is selected to be used.
  struct BrightnessStep {
    double target_percent;
    int decrease_threshold;
    int increase_threshold;
  };

  // Returns the total duration for |style|.
  static base::TimeDelta GetTransitionDuration(TransitionStyle style);

  // Returns the current time (or |current_time_for_testing_| if non-null).
  base::TimeTicks GetCurrentTime() const;

  void ReadPrefs();

  // Reads in the percentage limits for either the user control or the ALS
  // control. Takes in the contents of the pref file to a string of the format
  // "[<min target percent percent>\n<dim target percent>\n<max target
  // percent>\n]" and parses it into corresponding arguments. If this fails the
  // values of |min|, |dim|, and |max| are preserved.
  void ReadLimitsPrefs(const char* prefs_file,
                       double* min,
                       double* dim,
                       double* max);

  // Get the control values for the steps that the ALS control can move
  // in. Takes in the contents of the pref file to a string of the format
  // "[<target percent> <decrease threshold> <increase threshold>\n]+" and
  // parses it into a series of brightness steps.
  void ReadAlsStepsPref(const char* prefs_file);

  // Takes in the contents of the pref file to a string of the format "[<target
  // percent>\n]+" and parses it into a vector of doubles to be used as steps
  // that user commands change the keyboard brightness by.
  void ReadUserStepsPref(const char* prefs_file);

  // The backlight level and enabledness are separate values so that the code
  // that sets the level does not need to be aware of the fact the light in
  // certain circumstances might be disabled and the disable/enable code doesn't
  // need to be aware of the level logic.
  void UpdateBacklightEnabled();

  // Instantaneously sets the backlight device's brightness level.
  void WriteBrightnessLevel(int64 new_level);

  // Halt the video timeout timer and the callback for the video timeout.
  void HaltVideoTimeout();
  SIGNAL_CALLBACK_0(KeyboardBacklightController, gboolean, VideoTimeout);

  int64 PercentToLevel(double percent) const;
  double LevelToPercent(int64 level) const;

  // Until the user issues a command to set the backlight level
  // |user_step_index_| is set to -1, which is used as a signal about whether to
  // use |als_target_percent_| or |user_target_percent_|. To initialize this one
  // has to determine what the closet step for the user brightness to the
  // current level is and set the index appropriately.
  void InitializeUserStepIndex();

  // This method calculate from the current state of the system and values of
  // als_target_percent_ and user_target_percent_ the level that the backlight
  // should be set to.
  int64 GetNewLevel() const;

  bool is_initialized_;

  // Backlight used for dimming. Non-owned.
  system::BacklightInterface* backlight_;

  // Interface for saving preferences. Non-owned.
  PrefsInterface* prefs_;

  // Light sensor we need to register for updates from.  Non-owned.
  AmbientLightSensor* light_sensor_;

  // Observer that needs to be alerted of changes. Normally this is the powerd
  // daemon. Non-owned.
  BacklightControllerObserver* observer_;

  // Backlight power state, used to distinguish between various cases.
  // Backlight nonzero, backlight zero, backlight idle-dimmed, etc.
  PowerState state_;

  // State bits dealing with the video and full screen don't enable the
  // backlight case.
  bool is_video_playing_;
  bool is_fullscreen_;

  // Control value for if the backlight is enabled by the current video state.
  bool video_enabled_;

  // Maximum brightness level exposed by the backlight driver.
  // 0 is always the minimum.
  int64 max_level_;

  // Current level that the backlight is set to (or possibly in the process of
  // transitioning to).  This might vary from |target_percent_| due to the
  // backlight being disabled.
  int64 current_level_;

  // Current percentage that is trying to be set by the ALS, might not be set if
  // backlight is disabled.
  double als_target_percent_;

  // Current percentage that the user has defined.
  double user_target_percent_;

  // Control values used for defining the range that als control will set the
  // target brightness percent and a special value for dimming the backlight
  // when the device idles.
  double als_target_percent_dim_;
  double als_target_percent_max_;
  double als_target_percent_min_;

  // Control values used for defining the range that user control will set the
  // target brightness percent and a special value for dimming the backlight
  // when the device idles.
  double user_target_percent_dim_;
  double user_target_percent_max_;
  double user_target_percent_min_;

  // Apply temporal hysteresis to ALS values.
  AlsHysteresisState hysteresis_state_;
  int hysteresis_count_;

  // Most recent value received from the ALS.
  int lux_level_;

  // Current brightness step being used by the ALS.
  ssize_t als_step_index_;

  // Brightness step data that is read in from the prefs file. It is assumed
  // that this data is well formed, specifically for each entry in the file the
  // decrease thresholds are monotonically increasing and the increase
  // thresholds are monotonically decreasing. The value -1 is special and is
  // meant to indicate positive/negative infinity depending on the context.
  std::vector<BrightnessStep> als_steps_;

  // Current brightness step set by user, or -1 is |als_step_index_| should be
  // used.
  ssize_t user_step_index_;

  // Set of percentages that the user can select from for setting the
  // brightness. This is populated by either reading from a config value or
  // dropping in target_percent [min, dim, max] in.
  std::vector<double> user_steps_;

  // If true, we ignore readings from the ambient light sensor.  Controlled by
  // kDisableALSPref.
  bool ignore_ambient_light_;

  // Value returned when we add a timer for the video timeout. This is
  // needed for reseting the timer when the next event occurs.
  guint32 video_timeout_timer_id_;

  // Counters for stat tracking.
  int num_als_adjustments_;
  int num_user_adjustments_;

  // If non-null, used in place of base::TimeTicks::Now() when the current time
  // is needed.
  base::TimeTicks current_time_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardBacklightController);
};  // class KeyboardBacklightController

}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_KEYBOARD_BACKLIGHT_CONTROLLER_H_
