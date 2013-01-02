// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/policy/state_controller.h"

#include <inttypes.h>

#include <cmath>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "power_manager/common/prefs.h"
#include "power_manager/common/util.h"

namespace power_manager {
namespace policy {

namespace {

// Returns a human-readable description of |source|.
std::string PowerSourceToString(StateController::PowerSource source) {
  switch (source) {
    case StateController::POWER_AC:
      return "AC";
    case StateController::POWER_BATTERY:
      return "battery";
    default:
      return StringPrintf("unknown (%d)", source);
  }
}

// Returns a human-readable description of |state|.
std::string LidStateToString(StateController::LidState state) {
  switch (state) {
    case StateController::LID_OPEN:
      return "open";
    case StateController::LID_CLOSED:
      return "closed";
    default:
      return StringPrintf("unknown (%d)", state);
  }
}

// Returns a human-readable description of |state|.
std::string SessionStateToString(StateController::SessionState state) {
  switch (state) {
    case StateController::SESSION_STOPPED:
      return "stopped";
    case StateController::SESSION_STARTED:
      return "started";
    default:
      return StringPrintf("unknown (%d)", state);
  }
}

// Returns a human-readable description of |mode|.
std::string DisplayModeToString(StateController::DisplayMode mode) {
  switch (mode) {
    case StateController::DISPLAY_NORMAL:
      return "normal";
    case StateController::DISPLAY_PRESENTATION:
      return "presentation";
    default:
      return StringPrintf("unknown (%d)", mode);
  }
}

// Returns |delta| as a string of the format "3m45s".
std::string TimeDeltaToString(base::TimeDelta delta) {
  int64 seconds = llabs(delta.InSeconds());
  return StringPrintf("%s%" PRId64 "m%" PRId64 "s",
                      delta < base::TimeDelta() ? "-" : "",
                      seconds / 60, seconds % 60);
}

// Returns the time until an event occurring |delay| after |start| will
// happen, assuming that the current time is |now|.  Returns an empty
// base::TimeDelta if the event has already happened or happens at |now|.
base::TimeDelta GetRemainingTime(base::TimeTicks start,
                                 base::TimeTicks now,
                                 base::TimeDelta delay) {
  base::TimeTicks event_time = start + delay;
  return event_time > now ? event_time - now : base::TimeDelta();
}

// Returns the minimum positive value after comparing |a| and |b|.  If one
// is zero or negative, the other is returned.  If both are zero or
// negative, an empty base::TimeDelta is returned.
base::TimeDelta GetMinPositiveTimeDelta(base::TimeDelta a, base::TimeDelta b) {
  if (a > base::TimeDelta()) {
    if (b > base::TimeDelta()) {
      return a < b ? a : b;
    } else {
      return a;
    }
  } else {
    return b;
  }
}

// Helper function for UpdateState.  The general pattern here is:
// - If |inactivity_duration| has reached |delay| and
//   |action_already_performed| says that the controller hasn't yet
//   performed the corresponding action, then run |callback| and set
//   |action_already_performed| to ensure that the action doesn't get
//   performed again the next time this is called.
// - If |delay| hasn't been reached, then run |undo_callback| if non-null
//   to undo the action if needed and reset |action_already_performed| so
//   that the action can be performed later.
void HandleDelay(base::TimeDelta delay,
                 base::TimeDelta inactivity_duration,
                 base::Closure callback,
                 base::Closure undo_callback,
                 const std::string& description,
                 const std::string& undo_description,
                 bool* action_already_performed) {
  if (delay > base::TimeDelta() && inactivity_duration >= delay) {
    if (!*action_already_performed) {
      VLOG(1) << description << " after "
              << TimeDeltaToString(inactivity_duration);
      callback.Run();
      *action_already_performed = true;
    }
  } else if (*action_already_performed) {
    if (!undo_callback.is_null()) {
      VLOG(1) << undo_description;
      undo_callback.Run();
    }
    *action_already_performed = false;
  }
}

// Looks up |name|, an int64 preference representing milliseconds, in
// |prefs|, and returns it as a base::TimeDelta.  Returns true on success.
bool GetMillisecondPref(PrefsInterface* prefs,
                        const std::string& name,
                        base::TimeDelta* out) {
  DCHECK(prefs);
  DCHECK(out);

  int64 int_value = 0;
  if (!prefs->GetInt64(name, &int_value))
    return false;

  *out = base::TimeDelta::FromMilliseconds(int_value);
  return true;
}

}  // namespace

StateController::TestApi::TestApi(StateController* controller)
    : controller_(controller) {
}

StateController::TestApi::~TestApi() {
  controller_ = NULL;
}

void StateController::TestApi::SetCurrentTime(base::TimeTicks current_time) {
  controller_->current_time_for_testing_ = current_time;
}

base::TimeTicks StateController::TestApi::GetTimeoutTime() {
  return controller_->timeout_time_for_testing_;
}

void StateController::TestApi::TriggerTimeout() {
  CHECK(controller_->timeout_id_);
  guint scheduled_id = controller_->timeout_id_;
  if (!controller_->HandleTimeout()) {
    // Since the GLib timeout didn't actually fire, we need to remove it
    // manually to ensure it won't be leaked.
    CHECK(controller_->timeout_id_ != scheduled_id);
    util::RemoveTimeout(&scheduled_id);
  }
}

const double StateController::kDefaultPresentationIdleDelayFactor = 2.0;

StateController::StateController(Delegate* delegate, PrefsInterface* prefs)
    : delegate_(delegate),
      prefs_(prefs),
      initialized_(false),
      timeout_id_(0),
      power_source_(POWER_AC),
      lid_state_(LID_OPEN),
      session_state_(SESSION_STOPPED),
      display_mode_(DISPLAY_NORMAL),
      screen_dimmed_(false),
      screen_turned_off_(false),
      requested_screen_lock_(false),
      idle_action_performed_(false),
      lid_closed_action_performed_(false),
      require_usb_input_device_to_suspend_(false),
      keep_screen_on_for_audio_(false),
      disable_idle_suspend_(false),
      idle_action_(DO_NOTHING),
      lid_closed_action_(DO_NOTHING),
      use_audio_activity_(true),
      use_video_activity_(true),
      override_screen_dim_(false),
      override_screen_off_(false),
      override_idle_suspend_(false),
      override_lid_suspend_(false) {
  prefs_->AddObserver(this);
}

StateController::~StateController() {
  util::RemoveTimeout(&timeout_id_);
  prefs_->RemoveObserver(this);
}

void StateController::Init(PowerSource power_source,
                           LidState lid_state,
                           SessionState session_state,
                           DisplayMode display_mode) {
  last_user_activity_time_ = GetCurrentTime();
  power_source_ = power_source;
  lid_state_ = lid_state;
  session_state_ = session_state;
  display_mode_ = display_mode;

  LoadPrefs();
  UpdateSettingsAndState();
  initialized_ = true;
}

void StateController::HandlePowerSourceChange(PowerSource source) {
  DCHECK(initialized_);
  if (source == power_source_)
    return;

  VLOG(1) << "Power source changed to " << PowerSourceToString(source);
  power_source_ = source;
  last_user_activity_time_ = GetCurrentTime();
  UpdateSettingsAndState();
}

void StateController::HandleLidStateChange(LidState state) {
  DCHECK(initialized_);
  if (state == lid_state_)
    return;

  VLOG(1) << "Lid state changed to " << LidStateToString(state);
  lid_state_ = state;
  if (state == LID_OPEN)
    last_user_activity_time_ = GetCurrentTime();
  UpdateState();
}

void StateController::HandleSessionStateChange(SessionState state) {
  DCHECK(initialized_);
  if (state == session_state_)
    return;

  VLOG(1) << "Session state changed to " << SessionStateToString(state);
  session_state_ = state;
  last_user_activity_time_ = GetCurrentTime();
  UpdateSettingsAndState();
}

void StateController::HandleDisplayModeChange(DisplayMode mode) {
  DCHECK(initialized_);
  if (mode == display_mode_)
    return;

  VLOG(1) << "Display mode changed to " << DisplayModeToString(mode);
  display_mode_ = mode;
  last_user_activity_time_ = GetCurrentTime();
  UpdateSettingsAndState();
}

void StateController::HandleResume() {
  DCHECK(initialized_);
  VLOG(1) << "System resumed";
  last_user_activity_time_ = GetCurrentTime();
  UpdateState();
}

void StateController::HandlePolicyChange(const PowerManagementPolicy& policy) {
  DCHECK(initialized_);
  VLOG(1) << "Received updated policy";
  policy_ = policy;
  UpdateSettingsAndState();
}

void StateController::HandleOverrideChange(bool override_screen_dim,
                                           bool override_screen_off,
                                           bool override_idle_suspend,
                                           bool override_lid_suspend) {
  DCHECK(initialized_);
  if (override_screen_dim == override_screen_dim_ &&
      override_screen_off == override_screen_off_ &&
      override_idle_suspend == override_idle_suspend_ &&
      override_lid_suspend == override_lid_suspend_)
    return;

  VLOG(1) << "Overrides changed (override_dim=" << override_screen_dim
          << ", override_off=" << override_screen_off
          << ", override_idle_suspend=" << override_idle_suspend
          << ", override_lid_suspend=" << override_lid_suspend << ")";
  override_screen_dim_ = override_screen_dim;
  override_screen_off_ = override_screen_off;
  override_idle_suspend_ = override_idle_suspend;
  override_lid_suspend_ = override_lid_suspend;
  UpdateSettingsAndState();
}

void StateController::HandleUserActivity() {
  DCHECK(initialized_);
  VLOG(1) << "Saw user activity";
  last_user_activity_time_ = GetCurrentTime();
  UpdateState();
}

void StateController::HandleVideoActivity() {
  DCHECK(initialized_);
  VLOG(1) << "Saw video activity";
  last_video_activity_time_ = GetCurrentTime();
  UpdateState();
}

void StateController::HandleAudioActivity() {
  DCHECK(initialized_);
  VLOG(1) << "Saw audio activity";
  last_audio_activity_time_ = GetCurrentTime();
  UpdateState();
}

void StateController::OnPrefChanged(const std::string& pref_name) {
  DCHECK(initialized_);
  // The lock-on-suspend pref is the only one that's updated at runtime.
  if (pref_name == kLockOnIdleSuspendPref) {
    VLOG(1) << "Reloading prefs";
    LoadPrefs();
    UpdateSettingsAndState();
  }
}

// static
std::string StateController::ActionToString(Action action) {
  switch (action) {
    case SUSPEND:
      return "suspend";
    case STOP_SESSION:
      return "logout";
    case SHUT_DOWN:
      return "shutdown";
    case DO_NOTHING:
      return "no-op";
    default:
      return StringPrintf("unknown (%d)", action);
  }
}

// static
StateController::Action StateController::ProtoActionToAction(
    PowerManagementPolicy_Action proto_action) {
  switch (proto_action) {
    case PowerManagementPolicy_Action_SUSPEND:
      return SUSPEND;
    case PowerManagementPolicy_Action_STOP_SESSION:
      return STOP_SESSION;
    case PowerManagementPolicy_Action_SHUT_DOWN:
      return SHUT_DOWN;
    case PowerManagementPolicy_Action_DO_NOTHING:
      return DO_NOTHING;
    default:
      NOTREACHED() << "Unhandled action " << proto_action;
      return DO_NOTHING;
  }
}

// static
void StateController::SanitizeDelays(Delays* delays) {
  DCHECK(delays);

  // Don't try to turn the screen off after performing the idle action.
  if (delays->screen_off > base::TimeDelta())
    delays->screen_off = std::min(delays->screen_off, delays->idle);
  else
    delays->screen_off = base::TimeDelta();

  // Similarly, don't try to dim the screen after turning it off.
  if (delays->screen_dim > base::TimeDelta()) {
    delays->screen_dim = std::min(
        delays->screen_dim,
        GetMinPositiveTimeDelta(delays->idle, delays->screen_off));
  } else {
    delays->screen_dim = base::TimeDelta();
  }

  // If the lock delay matches or exceeds the idle delay, unset it --
  // Chrome's lock-before-suspend setting should be enabled instead.
  if (delays->screen_lock >= delays->idle ||
      delays->screen_lock < base::TimeDelta()) {
    delays->screen_lock = base::TimeDelta();
  }
}

// static
void StateController::MergeDelaysFromPolicy(
    const PowerManagementPolicy::Delays& policy_delays,
    Delays* delays_out) {
  DCHECK(delays_out);

  if (policy_delays.has_idle_ms() && policy_delays.idle_ms() >= 0) {
    delays_out->idle =
        base::TimeDelta::FromMilliseconds(policy_delays.idle_ms());
  }
  if (policy_delays.has_screen_dim_ms() && policy_delays.screen_dim_ms() >= 0) {
    delays_out->screen_dim =
        base::TimeDelta::FromMilliseconds(policy_delays.screen_dim_ms());
  }
  if (policy_delays.has_screen_off_ms() && policy_delays.screen_off_ms() >= 0) {
    delays_out->screen_off =
        base::TimeDelta::FromMilliseconds(policy_delays.screen_off_ms());
  }
  if (policy_delays.has_screen_lock_ms() &&
      policy_delays.screen_lock_ms() >= 0) {
    delays_out->screen_lock =
        base::TimeDelta::FromMilliseconds(policy_delays.screen_lock_ms());
  }
}

base::TimeTicks StateController::GetCurrentTime() const {
  return !current_time_for_testing_.is_null() ?
      current_time_for_testing_ : base::TimeTicks::Now();
}

base::TimeTicks StateController::GetLastActivityTimeForIdle() const {
  base::TimeTicks last_time = last_user_activity_time_;
  if (use_audio_activity_)
    last_time = std::max(last_time, last_audio_activity_time_);
  if (use_video_activity_)
    last_time = std::max(last_time, last_video_activity_time_);
  return last_time;
}

base::TimeTicks StateController::GetLastActivityTimeForScreenDimOrLock() const {
  base::TimeTicks last_time = last_user_activity_time_;
  if (use_video_activity_)
    last_time = std::max(last_time, last_video_activity_time_);
  return last_time;
}

base::TimeTicks StateController::GetLastActivityTimeForScreenOff() const {
  base::TimeTicks last_time = last_user_activity_time_;
  if (use_video_activity_)
    last_time = std::max(last_time, last_video_activity_time_);
  if (keep_screen_on_for_audio_)
    last_time = std::max(last_time, last_audio_activity_time_);
  return last_time;
}

void StateController::LoadPrefs() {
  prefs_->GetBool(kRequireUsbInputDeviceToSuspendPref,
                  &require_usb_input_device_to_suspend_);
  prefs_->GetBool(kKeepBacklightOnForAudioPref, &keep_screen_on_for_audio_);
  prefs_->GetBool(kDisableIdleSuspendPref, &disable_idle_suspend_);

  CHECK(GetMillisecondPref(prefs_, kPluggedSuspendMsPref,
                           &pref_ac_delays_.idle));
  CHECK(GetMillisecondPref(prefs_, kPluggedOffMsPref,
                           &pref_ac_delays_.screen_off));
  CHECK(GetMillisecondPref(prefs_, kPluggedDimMsPref,
                           &pref_ac_delays_.screen_dim));

  CHECK(GetMillisecondPref(prefs_, kUnpluggedSuspendMsPref,
                           &pref_battery_delays_.idle));
  CHECK(GetMillisecondPref(prefs_, kUnpluggedOffMsPref,
                           &pref_battery_delays_.screen_off));
  CHECK(GetMillisecondPref(prefs_, kUnpluggedDimMsPref,
                           &pref_battery_delays_.screen_dim));

  bool lock_on_idle_suspend = false;
  prefs_->GetBool(kLockOnIdleSuspendPref, &lock_on_idle_suspend);

  base::TimeDelta screen_lock_delay;
  GetMillisecondPref(prefs_, kLockMsPref, &screen_lock_delay);

  pref_ac_delays_.screen_lock = base::TimeDelta();
  pref_battery_delays_.screen_lock = base::TimeDelta();
  if (lock_on_idle_suspend && screen_lock_delay > base::TimeDelta()) {
    if (screen_lock_delay < pref_ac_delays_.idle)
      pref_ac_delays_.screen_lock = screen_lock_delay;
    if (screen_lock_delay < pref_battery_delays_.idle)
      pref_battery_delays_.screen_lock = screen_lock_delay;
  }

  SanitizeDelays(&pref_ac_delays_);
  SanitizeDelays(&pref_battery_delays_);
}

void StateController::UpdateSettingsAndState() {
  Action old_lid_closed_action = lid_closed_action_;

  // Start out with the defaults loaded from the power manager's prefs.
  idle_action_ = session_state_ == SESSION_STARTED ? SUSPEND : SHUT_DOWN;
  lid_closed_action_ = session_state_ == SESSION_STARTED ? SUSPEND : SHUT_DOWN;
  delays_ = power_source_ == POWER_AC ? pref_ac_delays_ : pref_battery_delays_;
  use_audio_activity_ = true;
  use_video_activity_ = true;

  // Now update them with values that were set in the policy.
  if (policy_.has_idle_action())
    idle_action_ = ProtoActionToAction(policy_.idle_action());
  if (policy_.has_lid_closed_action())
    lid_closed_action_ = ProtoActionToAction(policy_.lid_closed_action());

  switch (power_source_) {
    case POWER_AC:
      if (policy_.has_ac_delays())
        MergeDelaysFromPolicy(policy_.ac_delays(), &delays_);
      break;
    case POWER_BATTERY:
      if (policy_.has_battery_delays())
        MergeDelaysFromPolicy(policy_.battery_delays(), &delays_);
      break;
    default:
      NOTREACHED() << "Unhandled power source " << power_source_;
  }

  if (policy_.has_use_audio_activity())
    use_audio_activity_ = policy_.use_audio_activity();
  if (policy_.has_use_video_activity())
    use_video_activity_ = policy_.use_video_activity();

  if (display_mode_ == DISPLAY_PRESENTATION) {
    double presentation_factor =
        policy_.has_presentation_idle_delay_factor() ?
        policy_.presentation_idle_delay_factor() :
        kDefaultPresentationIdleDelayFactor;
    presentation_factor = std::max(presentation_factor, 1.0);
    base::TimeDelta orig_idle = delays_.idle;
    delays_.idle *= presentation_factor;
    if (delays_.screen_off > base::TimeDelta())
      delays_.screen_off = delays_.idle - (orig_idle - delays_.screen_off);
    if (delays_.screen_dim > base::TimeDelta())
      delays_.screen_dim = delays_.idle - (orig_idle - delays_.screen_dim);
    if (delays_.screen_lock > base::TimeDelta())
      delays_.screen_lock = delays_.idle - (orig_idle - delays_.screen_lock);
  }

  // The disable-idle-suspend pref overrides |policy_|.
  if (disable_idle_suspend_ && idle_action_ == SUSPEND)
    idle_action_ = DO_NOTHING;

  // Finally, apply temporary state overrides.
  // Screen-related overrides are only honored if the external policy
  // has not specified that video activity should be disregarded.
  if (use_video_activity_) {
    if (override_screen_dim_)
      delays_.screen_dim = base::TimeDelta();
    if (override_screen_off_)
      delays_.screen_off = base::TimeDelta();
    if (override_screen_dim_ || override_screen_off_)
      delays_.screen_lock = base::TimeDelta();
  }
  if (override_idle_suspend_ && idle_action_ == SUSPEND)
    idle_action_ = DO_NOTHING;
  if (override_lid_suspend_ && lid_closed_action_ == SUSPEND)
    lid_closed_action_ = DO_NOTHING;

  // If the lid-closed action changed, make sure that we perform the new
  // action in the event that the lid is already closed.
  if (lid_closed_action_ != old_lid_closed_action)
    lid_closed_action_performed_ = false;

  SanitizeDelays(&delays_);

  VLOG(1) << "Updated settings:"
          << " idle_action=" << ActionToString(idle_action_)
          << " lid_closed_action=" << ActionToString(lid_closed_action_)
          << " screen_dim_ms=" << delays_.screen_dim.InMilliseconds()
          << " screen_off_ms=" << delays_.screen_off.InMilliseconds()
          << " screen_lock_ms=" << delays_.screen_lock.InMilliseconds()
          << " idle_ms=" << delays_.idle.InMilliseconds()
          << " use_audio_activity=" << use_audio_activity_
          << " use_video_activity=" << use_video_activity_;

  UpdateState();
}

void StateController::PerformAction(Action action) {
  switch (action) {
    case SUSPEND:
      delegate_->Suspend();
      break;
    case STOP_SESSION:
      delegate_->StopSession();
      break;
    case SHUT_DOWN:
      delegate_->ShutDown();
      break;
    case DO_NOTHING:
      break;
    default:
      NOTREACHED() << "Unhandled action " << action;
  }
}

void StateController::UpdateState() {
  base::TimeTicks now = GetCurrentTime();
  base::TimeDelta idle_duration = now - GetLastActivityTimeForIdle();
  base::TimeDelta screen_dim_or_lock_duration =
      now - GetLastActivityTimeForScreenDimOrLock();
  base::TimeDelta screen_off_duration = now - GetLastActivityTimeForScreenOff();

  HandleDelay(delays_.screen_dim, screen_dim_or_lock_duration,
              base::Bind(&Delegate::DimScreen, base::Unretained(delegate_)),
              base::Bind(&Delegate::UndimScreen, base::Unretained(delegate_)),
              "Dimming screen", "Undimming screen", &screen_dimmed_);
  HandleDelay(delays_.screen_off, screen_off_duration,
              base::Bind(&Delegate::TurnScreenOff, base::Unretained(delegate_)),
              base::Bind(&Delegate::TurnScreenOn, base::Unretained(delegate_)),
              "Turning screen off", "Turning screen on", &screen_turned_off_);
  HandleDelay(delays_.screen_lock, screen_dim_or_lock_duration,
              base::Bind(&Delegate::LockScreen, base::Unretained(delegate_)),
              base::Closure(), "Locking screen", "", &requested_screen_lock_);

  Action idle_action_to_perform = DO_NOTHING;
  if (idle_duration >= delays_.idle) {
    if (!idle_action_performed_) {
      idle_action_to_perform = idle_action_;
      if (!delegate_->IsOobeCompleted()) {
        VLOG(1) << "Not performing idle action without OOBE completed";
        idle_action_to_perform = DO_NOTHING;
      }
      if (idle_action_to_perform == SUSPEND &&
          require_usb_input_device_to_suspend_ &&
          !delegate_->IsUsbInputDeviceConnected()) {
        VLOG(1) << "Not suspending for idle without USB input device";
        idle_action_to_perform = DO_NOTHING;
      }
      VLOG(1) << "Ready to perform idle action ("
              << ActionToString(idle_action_to_perform) << ") after "
              << TimeDeltaToString(idle_duration);
      idle_action_performed_ = true;
    }
  } else {
    idle_action_performed_ = false;
  }

  Action lid_closed_action_to_perform = DO_NOTHING;
  if (lid_state_ == LID_CLOSED) {
    if (!lid_closed_action_performed_) {
      lid_closed_action_to_perform = lid_closed_action_;
      VLOG(1) << "Ready to perform lid-closed action ("
              << ActionToString(lid_closed_action_to_perform) << ")";
      lid_closed_action_performed_ = true;
    }
  } else {
    lid_closed_action_performed_ = false;
  }

  if (idle_action_to_perform == SHUT_DOWN ||
      lid_closed_action_to_perform == SHUT_DOWN) {
    // If either of the actions is shutting down, don't perform the other.
    PerformAction(SHUT_DOWN);
  } else if (idle_action_to_perform == lid_closed_action_to_perform) {
    // If both actions are the same, only perform it once.
    PerformAction(idle_action_to_perform);
  } else {
    // Otherwise, perform both actions.  Note that one or both may be
    // DO_NOTHING.
    PerformAction(idle_action_to_perform);
    PerformAction(lid_closed_action_to_perform);
  }

  ScheduleTimeout(now);
}

void StateController::ScheduleTimeout(base::TimeTicks now) {
  base::TimeDelta timeout_delay;

  // Find the minimum of the delays that hasn't yet occurred.
  timeout_delay = GetMinPositiveTimeDelta(timeout_delay,
      GetRemainingTime(GetLastActivityTimeForScreenDimOrLock(), now,
                       delays_.screen_dim));
  timeout_delay = GetMinPositiveTimeDelta(timeout_delay,
      GetRemainingTime(GetLastActivityTimeForScreenOff(), now,
                       delays_.screen_off));
  timeout_delay = GetMinPositiveTimeDelta(timeout_delay,
      GetRemainingTime(GetLastActivityTimeForScreenDimOrLock(), now,
                       delays_.screen_lock));
  timeout_delay = GetMinPositiveTimeDelta(timeout_delay,
      GetRemainingTime(GetLastActivityTimeForIdle(), now, delays_.idle));

  util::RemoveTimeout(&timeout_id_);
  timeout_time_for_testing_ = base::TimeTicks();
  if (timeout_delay > base::TimeDelta()) {
    timeout_id_ = g_timeout_add(timeout_delay.InMilliseconds(),
                                &StateController::HandleTimeoutThunk, this);
    timeout_time_for_testing_ = now + timeout_delay;
  }
}

gboolean StateController::HandleTimeout() {
  timeout_id_ = 0;
  timeout_time_for_testing_ = base::TimeTicks();
  UpdateState();
  return FALSE;
}

}  // namespace policy
}  // namespace power_manager
