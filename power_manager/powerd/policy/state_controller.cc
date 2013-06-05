// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/policy/state_controller.h"

#include <inttypes.h>

#include <cmath>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "power_manager/common/clock.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/common/prefs.h"
#include "power_manager/common/util.h"

namespace power_manager {
namespace policy {

namespace {

// Returns |delta| as a string of the format "3m45s".
std::string TimeDeltaToString(base::TimeDelta delta) {
  int64 seconds = llabs(delta.InSeconds());
  return StringPrintf("%s%" PRId64 "m%" PRId64 "s",
                      delta < base::TimeDelta() ? "-" : "",
                      seconds / 60, seconds % 60);
}

// Returns |time_ms|, a time in milliseconds, as a
// TimeDeltaToString()-style string.
std::string MsToString(int64 time_ms) {
  return TimeDeltaToString(base::TimeDelta::FromMilliseconds(time_ms));
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
  controller_->clock_->set_current_time_for_testing(current_time);
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
const int StateController::kUserActivityAfterScreenOffIncreaseDelaysMs = 60000;

StateController::StateController(Delegate* delegate, PrefsInterface* prefs)
    : delegate_(delegate),
      prefs_(prefs),
      clock_(new Clock),
      initialized_(false),
      timeout_id_(0),
      power_source_(POWER_AC),
      lid_state_(LID_NOT_PRESENT),
      session_state_(SESSION_STOPPED),
      updater_state_(UPDATER_IDLE),
      display_mode_(DISPLAY_NORMAL),
      screen_dimmed_(false),
      screen_turned_off_(false),
      requested_screen_lock_(false),
      sent_idle_warning_(false),
      idle_action_performed_(false),
      lid_closed_action_performed_(false),
      turned_panel_off_for_docked_mode_(false),
      saw_user_activity_soon_after_screen_dim_or_off_(false),
      require_usb_input_device_to_suspend_(false),
      keep_screen_on_for_audio_(false),
      disable_idle_suspend_(false),
      suspend_at_login_screen_(false),
      allow_docked_mode_(false),
      ignore_external_policy_(false),
      idle_action_(DO_NOTHING),
      lid_closed_action_(DO_NOTHING),
      use_audio_activity_(true),
      use_video_activity_(true) {
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
  LoadPrefs();

  last_user_activity_time_ = clock_->GetCurrentTime();
  power_source_ = power_source;
  lid_state_ = lid_state;
  session_state_ = session_state;
  display_mode_ = display_mode;

  UpdateSettingsAndState();
  initialized_ = true;
}

void StateController::HandlePowerSourceChange(PowerSource source) {
  DCHECK(initialized_);
  if (source == power_source_)
    return;

  VLOG(1) << "Power source changed to " << PowerSourceToString(source);
  power_source_ = source;
  UpdateLastUserActivityTime();
  UpdateSettingsAndState();
}

void StateController::HandleLidStateChange(LidState state) {
  DCHECK(initialized_);
  if (state == lid_state_)
    return;

  VLOG(1) << "Lid state changed to " << LidStateToString(state);
  lid_state_ = state;
  if (state == LID_OPEN)
    UpdateLastUserActivityTime();
  UpdateState();
}

void StateController::HandleSessionStateChange(SessionState state) {
  DCHECK(initialized_);
  if (state == session_state_)
    return;

  VLOG(1) << "Session state changed to " << SessionStateToString(state);
  session_state_ = state;
  saw_user_activity_soon_after_screen_dim_or_off_ = false;
  UpdateLastUserActivityTime();
  UpdateSettingsAndState();
}

void StateController::HandleUpdaterStateChange(UpdaterState state) {
  DCHECK(initialized_);
  if (state == updater_state_)
    return;

  VLOG(1) << "Updater state changed to " << UpdaterStateToString(state);
  updater_state_ = state;
  UpdateSettingsAndState();
}

void StateController::HandleDisplayModeChange(DisplayMode mode) {
  DCHECK(initialized_);
  if (mode == display_mode_)
    return;

  VLOG(1) << "Display mode changed to " << DisplayModeToString(mode);
  display_mode_ = mode;
  UpdateLastUserActivityTime();
  UpdateSettingsAndState();
}

void StateController::HandleResume() {
  DCHECK(initialized_);
  VLOG(1) << "System resumed";

  switch (delegate_->QueryLidState()) {
    case LID_OPEN:  // fallthrough
    case LID_NOT_PRESENT:
      // Undim the screen and turn it back on immediately after the user
      // opens the lid or wakes the system through some other means.
      UpdateLastUserActivityTime();
      break;
    case LID_CLOSED:
      // If the lid is closed to suspend the machine and then very quickly
      // opened and closed again, the machine may resume without lid-opened
      // and lid-closed events being generated.  Ensure that we're able to
      // resuspend immediately in this case.
      if (lid_state_ == LID_CLOSED &&
          lid_closed_action_ == SUSPEND &&
          lid_closed_action_performed_) {
        VLOG(1) << "Lid still closed after resuming from lid-close-triggered "
                << "suspend; repeating lid-closed action";
        lid_closed_action_performed_ = false;
      }
      break;
  }

  UpdateState();
}

void StateController::HandlePolicyChange(const PowerManagementPolicy& policy) {
  DCHECK(initialized_);
  VLOG(1) << "Received updated external policy: "
          << GetPolicyDebugString(policy);
  policy_ = policy;
  UpdateSettingsAndState();
}

void StateController::HandleUserActivity() {
  DCHECK(initialized_);
  VLOG(1) << "Saw user activity";

  // Ignore user activity reported while the lid is closed unless we're in
  // docked mode.
  if (lid_state_ == LID_CLOSED && !in_docked_mode()) {
    LOG(WARNING) << "Ignoring user activity received while lid is closed";
    return;
  }

  const bool old_saw_user_activity =
      saw_user_activity_soon_after_screen_dim_or_off_;
  const bool screen_turned_off_recently =
      delays_.screen_off > base::TimeDelta() && screen_turned_off_ &&
      (clock_->GetCurrentTime() - screen_turned_off_time_).InMilliseconds() <=
      kUserActivityAfterScreenOffIncreaseDelaysMs;
  if (!saw_user_activity_soon_after_screen_dim_or_off_ &&
      ((screen_dimmed_ && !screen_turned_off_) || screen_turned_off_recently)) {
    VLOG(1) << "Scaling delays due to user activity while screen was dimmed "
            << "or soon after it was turned off";
    saw_user_activity_soon_after_screen_dim_or_off_ = true;
  }

  UpdateLastUserActivityTime();
  if (old_saw_user_activity != saw_user_activity_soon_after_screen_dim_or_off_)
    UpdateSettingsAndState();
  else
    UpdateState();
}

void StateController::HandleVideoActivity() {
  DCHECK(initialized_);
  VLOG(1) << "Saw video activity";
  last_video_activity_time_ = clock_->GetCurrentTime();
  UpdateState();
}

void StateController::HandleAudioActivity() {
  DCHECK(initialized_);
  VLOG(1) << "Saw audio activity";
  last_audio_activity_time_ = clock_->GetCurrentTime();
  UpdateState();
}

void StateController::AddIdleNotification(base::TimeDelta delay) {
  DCHECK(initialized_);
  if (delay <= base::TimeDelta()) {
    LOG(WARNING) << "Ignoring idle notification request for "
                 << TimeDeltaToString(delay);
    return;
  }
  VLOG(1) << "Adding idle notification for " << TimeDeltaToString(delay);
  pending_idle_notifications_.insert(delay);
  UpdateState();
}

void StateController::OnPrefChanged(const std::string& pref_name) {
  DCHECK(initialized_);
  if (pref_name == kDisableIdleSuspendPref ||
      pref_name == kIgnoreExternalPolicyPref) {
    VLOG(1) << "Reloading prefs for " << pref_name << " change";
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
std::string StateController::GetPolicyDelaysDebugString(
    const PowerManagementPolicy::Delays& delays,
    const std::string& prefix) {
  std::string str;
  if (delays.has_screen_dim_ms())
    str += prefix + "_dim=" + MsToString(delays.screen_dim_ms()) + " ";
  if (delays.has_screen_off_ms())
    str += prefix + "_screen_off=" + MsToString(delays.screen_off_ms()) + " ";
  if (delays.has_screen_lock_ms())
    str += prefix + "_lock=" + MsToString(delays.screen_lock_ms()) + " ";
  if (delays.has_idle_warning_ms())
    str += prefix + "_idle_warn=" + MsToString(delays.idle_warning_ms()) + " ";
  if (delays.has_idle_ms())
    str += prefix + "_idle=" + MsToString(delays.idle_ms()) + " ";
  return str;
}

// static
std::string StateController::GetPolicyDebugString(
    const PowerManagementPolicy& policy) {
  std::string str = GetPolicyDelaysDebugString(policy.ac_delays(), "ac");
  str += GetPolicyDelaysDebugString(policy.battery_delays(), "battery");

  if (policy.has_idle_action()) {
    str += "idle=" +
        ActionToString(ProtoActionToAction(policy.idle_action())) + " ";
  }
  if (policy.has_lid_closed_action()) {
    str += "lid_closed=" +
        ActionToString(ProtoActionToAction(policy.lid_closed_action())) + " ";
  }
  if (policy.has_use_audio_activity())
    str += "use_audio=" + base::IntToString(policy.use_audio_activity()) + " ";
  if (policy.has_use_video_activity())
    str += "use_video=" + base::IntToString(policy.use_video_activity()) + " ";
  if (policy.has_presentation_idle_delay_factor()) {
    str += "presentation_factor=" +
        base::DoubleToString(policy.presentation_idle_delay_factor()) + " ";
  }
  if (policy.has_user_activity_screen_dim_delay_factor()) {
    str += "user_activity_factor=" + base::DoubleToString(
        policy.user_activity_screen_dim_delay_factor()) + " ";
  }

  if (policy.has_reason())
    str += "(" + policy.reason() + ")";

  return str.empty() ? "[empty]" : str;
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

  // Cap the idle-warning timeout to the idle-action timeout.
  if (delays->idle_warning > base::TimeDelta())
    delays->idle_warning = std::min(delays->idle_warning, delays->idle);
  else
    delays->idle_warning = base::TimeDelta();

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
  if (policy_delays.has_idle_warning_ms() &&
      policy_delays.idle_warning_ms() >= 0) {
    delays_out->idle_warning =
        base::TimeDelta::FromMilliseconds(policy_delays.idle_warning_ms());
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

void StateController::UpdateLastUserActivityTime() {
  last_user_activity_time_ = clock_->GetCurrentTime();
  delegate_->ReportUserActivityMetrics();
}

void StateController::LoadPrefs() {
  prefs_->GetBool(kRequireUsbInputDeviceToSuspendPref,
                  &require_usb_input_device_to_suspend_);
  prefs_->GetBool(kKeepBacklightOnForAudioPref, &keep_screen_on_for_audio_);
  prefs_->GetBool(kDisableIdleSuspendPref, &disable_idle_suspend_);
  prefs_->GetBool(kSuspendAtLoginScreenPref, &suspend_at_login_screen_);
  prefs_->GetBool(kIgnoreExternalPolicyPref, &ignore_external_policy_);
  prefs_->GetBool(kAllowDockedModePref, &allow_docked_mode_);

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

  SanitizeDelays(&pref_ac_delays_);
  SanitizeDelays(&pref_battery_delays_);
}

void StateController::UpdateSettingsAndState() {
  Action old_idle_action = idle_action_;
  Action old_lid_closed_action = lid_closed_action_;

  // Start out with the defaults loaded from the power manager's prefs.
  idle_action_ =
      (session_state_ == SESSION_STARTED || suspend_at_login_screen_) ?
      SUSPEND : SHUT_DOWN;
  lid_closed_action_ = session_state_ == SESSION_STARTED ? SUSPEND : SHUT_DOWN;
  delays_ = power_source_ == POWER_AC ? pref_ac_delays_ : pref_battery_delays_;
  use_audio_activity_ = true;
  use_video_activity_ = true;
  double presentation_factor = kDefaultPresentationIdleDelayFactor;
  double user_activity_factor = 1.0;

  // Now update them with values that were set in the policy.
  if (!ignore_external_policy_) {
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
    if (policy_.has_presentation_idle_delay_factor())
      presentation_factor = policy_.presentation_idle_delay_factor();
    if (policy_.has_user_activity_screen_dim_delay_factor())
      user_activity_factor = policy_.user_activity_screen_dim_delay_factor();
  }

  // TODO(derat): Replace this setting with one that behaves similarly to
  // |user_activity_factor| (i.e. scaling the screen-dimming delay rather
  // than the idle delay) and use shared code for both.
  if (display_mode_ == DISPLAY_PRESENTATION) {
    presentation_factor = std::max(presentation_factor, 1.0);
    base::TimeDelta orig_idle = delays_.idle;
    delays_.idle *= presentation_factor;
    if (delays_.screen_off > base::TimeDelta())
      delays_.screen_off = delays_.idle - (orig_idle - delays_.screen_off);
    if (delays_.screen_dim > base::TimeDelta())
      delays_.screen_dim = delays_.idle - (orig_idle - delays_.screen_dim);
    if (delays_.screen_lock > base::TimeDelta())
      delays_.screen_lock = delays_.idle - (orig_idle - delays_.screen_lock);
    if (delays_.idle_warning > base::TimeDelta())
      delays_.idle_warning = delays_.idle - (orig_idle - delays_.idle_warning);
  } else if (saw_user_activity_soon_after_screen_dim_or_off_ &&
             delays_.screen_dim > base::TimeDelta()) {
    user_activity_factor = std::max(user_activity_factor, 1.0);
    const base::TimeDelta orig_screen_dim = delays_.screen_dim;
    delays_.screen_dim *= user_activity_factor;

    const base::TimeDelta diff = delays_.screen_dim - orig_screen_dim;
    if (delays_.screen_off > base::TimeDelta())
      delays_.screen_off += diff;
    if (delays_.screen_lock > base::TimeDelta())
      delays_.screen_lock += diff;
    if (delays_.idle_warning > base::TimeDelta())
      delays_.idle_warning += diff;
    if (delays_.idle > base::TimeDelta())
      delays_.idle += diff;
  }

  // The disable-idle-suspend pref overrides |policy_|.  Note that it also
  // overrides non-suspend actions; it should e.g. block the system from
  // shutting down on idle if no session has been started.
  if (disable_idle_suspend_)
    idle_action_ = DO_NOTHING;

  // Avoid suspending or shutting down due to inactivity while a system
  // update is being applied on AC power so users on slow connections can
  // get updates.  Continue suspending on lid-close so users don't get
  // confused, though.
  if (updater_state_ == UPDATER_UPDATING && power_source_ == POWER_AC &&
      (idle_action_ == SUSPEND || idle_action_ == SHUT_DOWN))
    idle_action_ = DO_NOTHING;

  // Ignore the lid being closed while presenting to support docked mode.
  if (allow_docked_mode_ && display_mode_ == DISPLAY_PRESENTATION)
    lid_closed_action_ = DO_NOTHING;

  // If the idle or lid-closed actions changed, make sure that we perform
  // the new actions in the event that the system is already idle or the
  // lid is already closed.
  if (idle_action_ != old_idle_action)
    idle_action_performed_ = false;
  if (lid_closed_action_ != old_lid_closed_action)
    lid_closed_action_performed_ = false;

  SanitizeDelays(&delays_);

  VLOG(1) << "Updated settings:"
          << " dim=" << TimeDeltaToString(delays_.screen_dim)
          << " screen_off=" << TimeDeltaToString(delays_.screen_off)
          << " lock=" << TimeDeltaToString(delays_.screen_lock)
          << " idle_warn=" << TimeDeltaToString(delays_.idle_warning)
          << " idle=" << TimeDeltaToString(delays_.idle)
          << " (" << ActionToString(idle_action_) << ")"
          << " lid_closed=" << ActionToString(lid_closed_action_)
          << " use_audio=" << use_audio_activity_
          << " use_video=" << use_video_activity_;

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
  base::TimeTicks now = clock_->GetCurrentTime();
  base::TimeDelta idle_duration = now - GetLastActivityTimeForIdle();
  base::TimeDelta screen_dim_or_lock_duration =
      now - GetLastActivityTimeForScreenDimOrLock();
  base::TimeDelta screen_off_duration = now - GetLastActivityTimeForScreenOff();

  for (std::set<base::TimeDelta>::iterator it =
           pending_idle_notifications_.begin();
       it != pending_idle_notifications_.end(); ) {
    if (*it <= idle_duration) {
      VLOG(1) << "Emitting idle notification for " << TimeDeltaToString(*it);
      delegate_->EmitIdleNotification(*it);
      pending_idle_notifications_.erase(it++);
    } else {
      break;
    }
  }

  HandleDelay(delays_.screen_dim, screen_dim_or_lock_duration,
              base::Bind(&Delegate::DimScreen, base::Unretained(delegate_)),
              base::Bind(&Delegate::UndimScreen, base::Unretained(delegate_)),
              "Dimming screen", "Undimming screen", &screen_dimmed_);

  const bool screen_was_turned_off = screen_turned_off_;
  HandleDelay(delays_.screen_off, screen_off_duration,
              base::Bind(&Delegate::TurnScreenOff, base::Unretained(delegate_)),
              base::Bind(&Delegate::TurnScreenOn, base::Unretained(delegate_)),
              "Turning screen off", "Turning screen on", &screen_turned_off_);
  if (screen_turned_off_ && !screen_was_turned_off)
    screen_turned_off_time_ = now;
  else if (!screen_turned_off_)
    screen_turned_off_time_ = base::TimeTicks();

  HandleDelay(delays_.screen_lock, screen_dim_or_lock_duration,
              base::Bind(&Delegate::LockScreen, base::Unretained(delegate_)),
              base::Closure(), "Locking screen", "", &requested_screen_lock_);

  // The idle-imminent signal is only emitted if an idle action is set.
  if (delays_.idle_warning > base::TimeDelta() &&
      idle_duration >= delays_.idle_warning &&
      idle_action_ != DO_NOTHING) {
    if (!sent_idle_warning_) {
      VLOG(1) << "Emitting idle-imminent signal after "
              << TimeDeltaToString(idle_duration);
      delegate_->EmitIdleActionImminent();
      sent_idle_warning_ = true;
    }
  } else if (sent_idle_warning_) {
    sent_idle_warning_ = false;
    // When resetting the idle-warning trigger, only emit the idle-deferred
    // signal if the idle action hasn't been performed yet or if it was a
    // no-op action.
    if (!idle_action_performed_ || idle_action_ == DO_NOTHING) {
      VLOG(1) << "Emitting idle-deferred signal";
      delegate_->EmitIdleActionDeferred();
    }
  }

  bool docked = in_docked_mode();
  if (docked != turned_panel_off_for_docked_mode_) {
    VLOG(1) << "Turning panel " << (docked ? "off" : "on") << " after "
            << (docked ? "entering" : "leaving") << " docked mode";
    delegate_->UpdatePanelForDockedMode(docked);
    turned_panel_off_for_docked_mode_ = docked;
  }

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
      if (idle_action_to_perform == SUSPEND &&
          delegate_->ShouldAvoidSuspendForHeadphoneJack()) {
        VLOG(1) << "Not suspending for idle due to headphone jack";
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
      GetRemainingTime(GetLastActivityTimeForIdle(), now,
                       delays_.idle_warning));
  timeout_delay = GetMinPositiveTimeDelta(timeout_delay,
      GetRemainingTime(GetLastActivityTimeForIdle(), now, delays_.idle));

  if (!pending_idle_notifications_.empty()) {
    timeout_delay = GetMinPositiveTimeDelta(timeout_delay,
        GetRemainingTime(GetLastActivityTimeForIdle(), now,
                         *pending_idle_notifications_.begin()));
  }

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
