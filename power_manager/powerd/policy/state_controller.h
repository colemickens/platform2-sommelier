// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_POLICY_STATE_CONTROLLER_H_
#define POWER_MANAGER_POWERD_POLICY_STATE_CONTROLLER_H_
#pragma once

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/time.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/common/prefs_observer.h"
#include "power_manager/common/signal_callback.h"
#include "power_manager/policy.pb.h"

typedef int gboolean;
typedef unsigned int guint;

namespace power_manager {

class PrefsInterface;

namespace policy {

// StateController is responsible for telling the power manager when to
// perform various actions.
class StateController : public PrefsObserver {
 public:
  // Interface for classes that perform the actions requested by
  // StateController (or otherwise help it interact with the real world).
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Returns true if a USB input devices is connected.
    virtual bool IsUsbInputDeviceConnected() = 0;

    // Returns true if the Chrome OS OOBE (out of box experience) has been
    // completed.
    virtual bool IsOobeCompleted() = 0;

    // Returns true if the system should not be suspended due to something
    // being connected to the headphone jack (as a workaround for hardware
    // that produces noise over the jack while suspended).
    virtual bool ShouldAvoidSuspendForHeadphoneJack() = 0;

    // Returns the current lid state.
    virtual LidState QueryLidState() = 0;

    // Dims the screen in response to the system being idle.
    virtual void DimScreen() = 0;

    // Undoes DimScreen().
    virtual void UndimScreen() = 0;

    // Turns the screen off in response to the system being idle.
    virtual void TurnScreenOff() = 0;

    // Undoes TurnScreenOff().
    virtual void TurnScreenOn() = 0;

    // Requests that the screen be locked.
    virtual void LockScreen() = 0;

    // Suspends the system.
    virtual void Suspend() = 0;

    // Stops the current session, logging the currently-logged-in user out.
    virtual void StopSession() = 0;

    // Shuts the system down.
    virtual void ShutDown() = 0;

    // Announces that an idle notification requested via
    // AddIdleNotification() has been triggered.
    virtual void EmitIdleNotification(base::TimeDelta delay) = 0;

    // Reports metrics in response to user activity.
    virtual void ReportUserActivityMetrics() = 0;
  };

  class TestApi {
   public:
    explicit TestApi(StateController* controller);
    ~TestApi();

    // Sets a fake current time for |controller_|.
    void SetCurrentTime(base::TimeTicks current_time);

    // Returns the time at which |controller_|'s timeout is next scheduled to
    // fire, or base::TimeTicks() if it's unscheduled.
    base::TimeTicks GetTimeoutTime();

    // Runs StateController::HandleTimeout().  May only be called if a timeout
    // is actually registered.
    void TriggerTimeout();

   private:
    StateController* controller_;  // not owned

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  // Delays loaded from prefs are scaled by this amount while an external
  // display is connected.
  static const double kDefaultPresentationIdleDelayFactor;

  StateController(Delegate* delegate, PrefsInterface* prefs);
  ~StateController();

  base::TimeTicks last_user_activity_time() const {
    return last_user_activity_time_;
  }

  void Init(PowerSource power_source,
            LidState lid_state,
            SessionState session_state,
            DisplayMode display_mode);

  // Handles various changes to external state.
  void HandlePowerSourceChange(PowerSource source);
  void HandleLidStateChange(LidState state);
  void HandleSessionStateChange(SessionState state);
  void HandleDisplayModeChange(DisplayMode mode);
  void HandleResume();
  void HandlePolicyChange(const PowerManagementPolicy& policy);
  void HandleOverrideChange(bool override_screen_dim,
                            bool override_screen_off,
                            bool override_idle_suspend,
                            bool override_lid_suspend);

  // Handles notification of different types of activity.
  void HandleUserActivity();
  void HandleVideoActivity();
  void HandleAudioActivity();

  // Adds an idle notification on behalf of an external process.
  // TODO(derat): Kill this.  "Idle" is poorly defined here, e.g. should it
  // include video activity?  Chrome should handle this itself
  // (notifications are currently used by Chrome for kiosk mode and
  // screensavers).
  void AddIdleNotification(base::TimeDelta delay);

  // PrefsInterface::Observer implementation:
  virtual void OnPrefChanged(const std::string& pref_name) OVERRIDE;

 private:
  // Holds a collection of delays.
  struct Delays {
    base::TimeDelta idle;
    base::TimeDelta screen_off;
    base::TimeDelta screen_dim;
    base::TimeDelta screen_lock;
  };

  // These correspond to the identically-named values in the
  // PowerManagementPolicy_Action enum.
  enum Action {
    SUSPEND,
    STOP_SESSION,
    SHUT_DOWN,
    DO_NOTHING,
  };

  // Returns a human-readable description of |action|.
  static std::string ActionToString(Action action);

  // Converts an Action enum value from a PowerManagementPolicy protocol buffer
  // to the corresponding StateController::Action value.
  static Action ProtoActionToAction(PowerManagementPolicy_Action proto_action);

  // Returns a string describing |delays| with each field prefixed by
  // |prefix|.  Helper method for GetPolicyDebugString().
  static std::string GetPolicyDelaysDebugString(
      const PowerManagementPolicy::Delays& delays,
      const std::string& prefix);

  // Returns a string describing |policy|.
  static std::string GetPolicyDebugString(const PowerManagementPolicy& policy);

  // Adjusts values in |delays| to ensure they make sense.
  static void SanitizeDelays(Delays* delays);

  // Merges set fields from |policy_delays| into |delays_out|, which should
  // already be initialized with default settings.
  static void MergeDelaysFromPolicy(
      const PowerManagementPolicy::Delays& policy_delays,
      Delays* delays_out);

  // Returns the current time or |current_time_for_testing_| if set.
  base::TimeTicks GetCurrentTime() const;

  // Returns the last time at which activity occurred that should defer
  // |idle_action_|, taking |on_ac_|, |use_audio_activity_|, and
  // |use_video_activity_| into account.
  base::TimeTicks GetLastActivityTimeForIdle() const;

  // Returns the last time at which activity occurred that should defer the
  // screen getting dimmed or locked.
  base::TimeTicks GetLastActivityTimeForScreenDimOrLock() const;

  // Returns the last time at which activity occurred that should defer the
  // screen getting turned off.  This is generally the same as
  // GetLastActivityTimeForScreenDimOrLock() but may differ if
  // |keep_screen_on_for_audio_| is set.
  base::TimeTicks GetLastActivityTimeForScreenOff() const;

  // Updates |last_user_activity_time_| to contain the current time and
  // calls |delegate_|'s ReportUserActivityMetrics() method.
  void UpdateLastUserActivityTime();

  // Initializes |require_usb_input_device_to_suspend_| and |pref_*| from
  // |prefs_|.
  void LoadPrefs();

  // Updates in-use settings and calls UpdateState().  Copies values from
  // |pref_*| and then applies externally-provided settings from |policy_|.
  void UpdateSettingsAndState();

  // Instructs |delegate_| to perform |action|.
  void PerformAction(Action action);

  // Ensures that the system is in the correct state, given the times at
  // which activity was last seen, the lid state, the currently-set delays,
  // etc.  Invokes ScheduleTimeout() when done.  If something that affects
  // the current settings has changed, UpdateSettingsAndState() should be
  // called instead.
  void UpdateState();

  // Cancels |timeout_id_| and creates a new timeout that will fire when
  // action is next needed, given a current time of |now|.
  void ScheduleTimeout(base::TimeTicks now);

  // Invoked by |timeout_id_| when it's time to perform an action.
  SIGNAL_CALLBACK_0(StateController, gboolean, HandleTimeout);

  Delegate* delegate_;  // not owned

  PrefsInterface* prefs_;  // not owned

  // Has Init() been called?
  bool initialized_;

  // If non-empty, used in place of base::TimeTicks::Now() whenever the
  // current time is needed.
  base::TimeTicks current_time_for_testing_;

  // GLib source ID for running HandleTimeout().  0 if unset.
  guint timeout_id_;

  // Time at which |timeout_id_| has been scheduled to fire.
  base::TimeTicks timeout_time_for_testing_;

  // Current power source.
  PowerSource power_source_;

  // Current state of the lid.
  LidState lid_state_;

  // Current session state.
  SessionState session_state_;

  // Whether the system is presenting or not.
  DisplayMode display_mode_;

  // These track whether various actions have already been performed by
  // UpdateState().
  bool screen_dimmed_;
  bool screen_turned_off_;
  bool requested_screen_lock_;
  bool idle_action_performed_;
  bool lid_closed_action_performed_;

  // Should the system only idle-suspend if a USB input device is
  // connected?  This is controlled by the
  // |kRequireUsbInputDeviceToSuspendPref| pref and set on hardware that
  // doesn't wake in response to Bluetooth input devices.
  bool require_usb_input_device_to_suspend_;

  // Should the screen be kept on while audio is playing?  This controlled
  // by the |kKeepBacklightOnForAudioPref| pref and set for hardware that
  // is unable to produce audio while the screen is turned off.
  bool keep_screen_on_for_audio_;

  // Should the system be prevented from suspending in response to
  // inactivity?  This is controlled by the |kDisableIdleSuspendPref| pref
  // and overrides |policy_|.
  bool disable_idle_suspend_;

  // Does the system have a lid?
  bool has_lid_;

  // Externally-requested idle notifications added via
  // AddIdleNotification() that haven't yet fired.  (Notifications are only
  // sent once.)
  std::set<base::TimeDelta> pending_idle_notifications_;

  base::TimeTicks last_user_activity_time_;
  base::TimeTicks last_video_activity_time_;
  base::TimeTicks last_audio_activity_time_;

  // Most recent externally-supplied policy.
  PowerManagementPolicy policy_;

  // Current settings (|pref_*| with |policy_| layered on top).
  Action idle_action_;
  Action lid_closed_action_;
  Delays delays_;
  bool use_audio_activity_;
  bool use_video_activity_;

  // Default settings loaded from prefs.
  Delays pref_ac_delays_;
  Delays pref_battery_delays_;

  // Temporary state overrides passed via StateOverrideRequest.
  // TODO(derat): Make Chrome use PowerManagementPolicy for this instead.
  bool override_screen_dim_;
  bool override_screen_off_;
  bool override_idle_suspend_;
  bool override_lid_suspend_;

  DISALLOW_COPY_AND_ASSIGN(StateController);
};

}  // namespace policy
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_POLICY_STATE_CONTROLLER_H_
