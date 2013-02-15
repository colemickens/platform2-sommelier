// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_KEYBOARD_BACKLIGHT_CONTROLLER_H_
#define POWER_MANAGER_POWERD_KEYBOARD_BACKLIGHT_CONTROLLER_H_

#include <glib.h>
#include <vector>

#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "base/time.h"
#include "power_manager/common/signal_callback.h"
#include "power_manager/powerd/backlight_controller.h"
#include "power_manager/powerd/system/ambient_light_observer.h"

namespace power_manager {

class KeyboardBacklightControllerTest;
class PrefsInterface;

namespace system {
class AmbientLightSensorInterface;
class BacklightInterface;
}  // namespace system

// Controls the keyboard backlight for devices with such a backlight.
class KeyboardBacklightController : public BacklightController,
                                    public system::AmbientLightObserver {
 public:
  // Helper class for tests that need to access internal state.
  class TestApi {
   public:
    explicit TestApi(KeyboardBacklightController* controller);
    ~TestApi();

    int64 lux_level() const { return controller_->lux_level_; }

    // Triggers |video_timeout_id_|, which must be set.
    void TriggerVideoTimeout();

   private:
    KeyboardBacklightController* controller_;  // not owned

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  KeyboardBacklightController(system::BacklightInterface* backlight,
                              PrefsInterface* prefs,
                              system::AmbientLightSensorInterface* sensor);
  virtual ~KeyboardBacklightController();

  // Initializes the object.
  bool Init();

  // Called when a notification about video activity has been received.
  void HandleVideoActivity(bool is_fullscreen);

  // BacklightController implementation:
  virtual void AddObserver(BacklightControllerObserver* observer) OVERRIDE;
  virtual void RemoveObserver(BacklightControllerObserver* observer) OVERRIDE;
  virtual void HandlePowerSourceChange(PowerSource source) OVERRIDE;
  virtual void SetDimmedForInactivity(bool dimmed) OVERRIDE;
  virtual void SetOffForInactivity(bool off) OVERRIDE;
  virtual void SetSuspended(bool suspended) OVERRIDE;
  virtual void SetShuttingDown(bool shutting_down) OVERRIDE;
  virtual bool GetBrightnessPercent(double* percent) OVERRIDE;
  virtual bool SetUserBrightnessPercent(double percent, TransitionStyle style)
      OVERRIDE;
  virtual bool IncreaseUserBrightness(bool only_if_zero) OVERRIDE;
  virtual bool DecreaseUserBrightness(bool allow_off) OVERRIDE;
  virtual int GetNumAmbientLightSensorAdjustments() const OVERRIDE;
  virtual int GetNumUserAdjustments() const OVERRIDE;

  // system::AmbientLightObserver implementation:
  virtual void OnAmbientLightChanged(
      system::AmbientLightSensorInterface* sensor) OVERRIDE;

 private:
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

  enum AlsHysteresisState {
    ALS_HYST_IDLE,
    ALS_HYST_DOWN,
    ALS_HYST_UP,
    ALS_HYST_IMMEDIATE,
  };

  void ReadPrefs();

  // Reads in the percentage limits for either the user control or the ALS
  // control. Takes in the contents of the pref file to a string of the
  // format "[<min percent percent>\n<dim percent>\n<max percent>\n]" and
  // parses it into corresponding arguments. If this fails the values of
  // |min|, |dim|, and |max| are preserved.
  void ReadLimitsPrefs(const std::string& pref_name,
                       double* min,
                       double* dim,
                       double* max);

  // Get the control values for the steps that the ALS control can move
  // in. Takes in the contents of the pref file to a string of the format
  // "[<target percent> <decrease threshold> <increase threshold>\n]+" and
  // parses it into a series of brightness steps.
  void ReadAlsStepsPref();

  // Takes in the contents of the pref file to a string of the format "[<target
  // percent>\n]+" and parses it into a vector of doubles to be used as steps
  // that user commands change the keyboard brightness by.
  void ReadUserStepsPref();

  // Handles |video_timeout_id_| firing, indicating that video activity has
  // stopped.
  SIGNAL_CALLBACK_0(KeyboardBacklightController, gboolean, HandleVideoTimeout);

  int64 PercentToLevel(double percent) const;
  double LevelToPercent(int64 level) const;

  // Returns the brightness from the current step in either |als_steps_| or
  // |user_steps_|, depending on which is in use.
  double GetUndimmedPercent() const;

  // Initializes |user_step_index_| when transitioning from ALS to user control.
  void InitUserStepIndex();

  // Passes GetUndimmedPercent() to ApplyBrightnessPercent() if currently
  // in a state where the undimmed brightness should be used.  Returns true
  // if the brightness was changed.
  bool UpdateUndimmedBrightness(TransitionStyle transition,
                                BrightnessChangeCause cause);

  // Updates the current brightness after assessing the current state
  // (based on |dimmed_for_inactivity_|, |off_for_inactivity_|, etc.).
  // Should be called whenever the state changes.
  bool UpdateState();

  // Sets the backlight's brightness to |percent| over |transition|.
  // Returns true and notifies observers if the brightness was changed.
  bool ApplyBrightnessPercent(double percent,
                              TransitionStyle transition,
                              BrightnessChangeCause cause);

  // Backlight used for dimming. Non-owned.
  system::BacklightInterface* backlight_;

  // Interface for saving preferences. Non-owned.
  PrefsInterface* prefs_;

  // Light sensor we need to register for updates from.  Non-owned.
  system::AmbientLightSensorInterface* light_sensor_;

  // Observers to notify about changes.
  ObserverList<BacklightControllerObserver> observers_;

  bool dimmed_for_inactivity_;
  bool off_for_inactivity_;
  bool shutting_down_;

  // Is a fullscreen video currently being played?
  bool fullscreen_video_playing_;

  // Maximum brightness level exposed by the backlight driver.
  // 0 is always the minimum.
  int64 max_level_;

  // Current level that |backlight_| is set to (or possibly in the process
  // of transitioning to).
  int64 current_level_;

  // Control values used for defining the range that als control will set the
  // target brightness percent and a special value for dimming the backlight
  // when the device idles.
  double als_percent_dim_;
  double als_percent_max_;
  double als_percent_min_;

  // Control values used for defining the range that user control will set the
  // target brightness percent and a special value for dimming the backlight
  // when the device idles.
  double user_percent_dim_;
  double user_percent_max_;
  double user_percent_min_;

  // Apply temporal hysteresis to ALS values.
  AlsHysteresisState hysteresis_state_;
  int hysteresis_count_;

  // Most recent value received from the ALS.
  int lux_level_;

  // Current brightness step within |als_steps_| being used by the ambient
  // light sensor.
  ssize_t als_step_index_;

  // Brightness step data that is read in from the prefs file. It is assumed
  // that this data is well formed, specifically for each entry in the file the
  // decrease thresholds are monotonically increasing and the increase
  // thresholds are monotonically decreasing. The value -1 is special and is
  // meant to indicate positive/negative infinity depending on the context.
  std::vector<BrightnessStep> als_steps_;

  // Current brightness step within |user_steps_| set by user, or -1 if
  // |als_step_index_| should be used.
  ssize_t user_step_index_;

  // Set of percentages that the user can select from for setting the
  // brightness. This is populated by either reading from a config value or
  // dropping in target_percent [min, dim, max] in.
  std::vector<double> user_steps_;

  // If true, we ignore readings from the ambient light sensor.  Controlled by
  // kDisableALSPref.
  bool ignore_ambient_light_;

  // GLib timeout ID for HandleVideoTimeout().
  guint32 video_timeout_id_;

  // Counters for stat tracking.
  int num_als_adjustments_;
  int num_user_adjustments_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardBacklightController);
};  // class KeyboardBacklightController

}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_KEYBOARD_BACKLIGHT_CONTROLLER_H_
