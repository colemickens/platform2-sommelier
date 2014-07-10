// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_POLICY_KEYBOARD_BACKLIGHT_CONTROLLER_H_
#define POWER_MANAGER_POWERD_POLICY_KEYBOARD_BACKLIGHT_CONTROLLER_H_

#include <vector>

#include <base/compiler_specific.h>
#include <base/memory/scoped_ptr.h>
#include <base/observer_list.h>
#include <base/time/time.h>
#include <base/timer/timer.h>

#include "power_manager/powerd/policy/ambient_light_handler.h"
#include "power_manager/powerd/policy/backlight_controller.h"
#include "power_manager/powerd/policy/backlight_controller_observer.h"

namespace power_manager {

class PrefsInterface;

namespace system {
class AmbientLightSensorInterface;
class BacklightInterface;
}  // namespace system

namespace policy {

class KeyboardBacklightControllerTest;

// Controls the keyboard backlight for devices with such a backlight.
class KeyboardBacklightController
    : public BacklightController,
      public AmbientLightHandler::Delegate,
      public BacklightControllerObserver {
 public:
  // Helper class for tests that need to access internal state.
  class TestApi {
   public:
    explicit TestApi(KeyboardBacklightController* controller);
    ~TestApi();

    // Triggers |video_timeout_id_|, which must be set.
    void TriggerVideoTimeout();

   private:
    KeyboardBacklightController* controller_;  // weak

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  // Backlight brightness percent to use when the screen is dimmed.
  static const double kDimPercent;

  KeyboardBacklightController();
  virtual ~KeyboardBacklightController();

  // Initializes the object. Ownership of passed-in pointers remains with the
  // caller. |sensor| and |display_backlight_controller| may be NULL.
  void Init(system::BacklightInterface* backlight,
            PrefsInterface* prefs,
            system::AmbientLightSensorInterface* sensor,
            BacklightController* display_backlight_controller);

  // Called when a notification about video activity has been received.
  void HandleVideoActivity(bool is_fullscreen);

  // BacklightController implementation:
  virtual void AddObserver(BacklightControllerObserver* observer) OVERRIDE;
  virtual void RemoveObserver(BacklightControllerObserver* observer) OVERRIDE;
  virtual void HandlePowerSourceChange(PowerSource source) OVERRIDE;
  virtual void HandleDisplayModeChange(DisplayMode mode) OVERRIDE;
  virtual void HandleSessionStateChange(SessionState state) OVERRIDE;
  virtual void HandlePowerButtonPress() OVERRIDE;
  virtual void HandleUserActivity(UserActivityType type) OVERRIDE;
  virtual void HandlePolicyChange(const PowerManagementPolicy& policy) OVERRIDE;
  virtual void HandleChromeStart() OVERRIDE;
  virtual void SetDimmedForInactivity(bool dimmed) OVERRIDE;
  virtual void SetOffForInactivity(bool off) OVERRIDE;
  virtual void SetSuspended(bool suspended) OVERRIDE;
  virtual void SetShuttingDown(bool shutting_down) OVERRIDE;
  virtual void SetDocked(bool docked) OVERRIDE;
  virtual bool GetBrightnessPercent(double* percent) OVERRIDE;
  virtual bool SetUserBrightnessPercent(double percent, TransitionStyle style)
      OVERRIDE;
  virtual bool IncreaseUserBrightness() OVERRIDE;
  virtual bool DecreaseUserBrightness(bool allow_off) OVERRIDE;
  virtual int GetNumAmbientLightSensorAdjustments() const OVERRIDE;
  virtual int GetNumUserAdjustments() const OVERRIDE;

  // AmbientLightHandler::Delegate implementation:
  virtual void SetBrightnessPercentForAmbientLight(
      double brightness_percent,
      AmbientLightHandler::BrightnessChangeCause cause) OVERRIDE;

  // BacklightControllerObserver implementation:
  virtual void OnBrightnessChanged(
      double brightness_percent,
      BacklightController::BrightnessChangeCause cause,
      BacklightController* source) OVERRIDE;

 private:
  // Handles |video_timer_| firing, indicating that video activity has stopped.
  void HandleVideoTimeout();

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

  // Backlight used for dimming. Weak pointer.
  system::BacklightInterface* backlight_;

  // Interface for saving preferences. Weak pointer.
  PrefsInterface* prefs_;

  // Controller responsible for the display's brightness. Weak pointer.
  BacklightController* display_backlight_controller_;

  scoped_ptr<AmbientLightHandler> ambient_light_handler_;

  // Observers to notify about changes.
  ObserverList<BacklightControllerObserver> observers_;

  SessionState session_state_;

  bool dimmed_for_inactivity_;
  bool off_for_inactivity_;
  bool shutting_down_;
  bool docked_;

  // Is a fullscreen video currently being played?
  bool fullscreen_video_playing_;

  // Maximum brightness level exposed by the backlight driver.
  // 0 is always the minimum.
  int64 max_level_;

  // Current level that |backlight_| is set to (or possibly in the process
  // of transitioning to).
  int64 current_level_;

  // Current brightness step within |user_steps_| set by user, or -1 if
  // |percent_for_ambient_light_| should be used.
  ssize_t user_step_index_;

  // Set of percentages that the user can select from for setting the
  // brightness. This is populated from a preference.
  std::vector<double> user_steps_;

  // Backlight brightness in the range [0.0, 100.0] to use when the ambient
  // light sensor is controlling the brightness.  This is set by
  // |ambient_light_handler_|.
  double percent_for_ambient_light_;

  // Runs HandleVideoTimeout().
  base::OneShotTimer<KeyboardBacklightController> video_timer_;

  // Counters for stat tracking.
  int num_als_adjustments_;
  int num_user_adjustments_;

  // Did |display_backlight_controller_| indicate that the display
  // backlight brightness is currently zero?
  bool display_brightness_is_zero_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardBacklightController);
};

}  // namespace policy
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_POLICY_KEYBOARD_BACKLIGHT_CONTROLLER_H_
