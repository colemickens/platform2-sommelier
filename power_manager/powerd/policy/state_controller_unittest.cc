// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdarg>
#include <inttypes.h>

#include <gtest/gtest.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "power_manager/common/fake_prefs.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/powerd/policy/state_controller.h"

namespace power_manager {
namespace policy {

namespace {

// Strings returned by TestDelegate::GetActions() to describe various
// actions that were requested.
const char kScreenDim[] = "dim";
const char kScreenOff[] = "off";
const char kScreenLock[] = "lock";
const char kScreenUndim[] = "undim";
const char kScreenOn[] = "on";
const char kSuspend[] = "suspend";
const char kStopSession[] = "logout";
const char kShutDown[] = "shutdown";
const char kReportUserActivityMetrics[] = "metrics";

// String returned by TestDelegate::GetActions() if no actions were
// requested.
const char kNoActions[] = "";

// Returns a string describing a TestDelegate::EmitIdleNotification() call
// with |delay|.
std::string GetEmitIdleNotificationAction(base::TimeDelta delay) {
  return StringPrintf("idle:%" PRId64, delay.InMilliseconds());
}

// Joins a sequence of strings describing actions (e.g. kScreenDim) such
// that they can be compared against a string returned by
// TestDelegate::GetActions().  The list of actions must be terminated by a
// NULL pointer.
std::string JoinActions(const char* action, ...) {
  std::string actions;

  va_list arg_list;
  va_start(arg_list, action);
  while (action) {
    if (!actions.empty())
      actions += ",";
    actions += action;
    action = va_arg(arg_list, const char*);
  }
  va_end(arg_list);
  return actions;
}

// StateController::Delegate implementation that records requested actions.
class TestDelegate : public StateController::Delegate {
 public:
  TestDelegate()
      : record_metrics_actions_(false),
        usb_input_device_connected_(false),
        oobe_completed_(true),
        avoid_suspend_for_headphones_(false),
        lid_state_(LID_OPEN) {
  }
  ~TestDelegate() {}

  void set_record_metrics_actions(bool record) {
    record_metrics_actions_ = record;
  }
  void set_usb_input_device_connected(bool connected) {
    usb_input_device_connected_ = connected;
  }
  void set_oobe_completed(bool completed) {
    oobe_completed_ = completed;
  }
  void set_avoid_suspend_for_headphones(bool avoid_suspend) {
    avoid_suspend_for_headphones_ = avoid_suspend;
  }
  void set_lid_state(LidState state) { lid_state_ = state; }

  // Returns a comma-separated string describing the actions that were
  // requested since the previous call to GetActions() (i.e. results are
  // non-repeatable).
  std::string GetActions() {
    std::string actions = actions_;
    actions_.clear();
    return actions;
  }

  // StateController::Delegate overrides:
  virtual bool IsUsbInputDeviceConnected() OVERRIDE {
    return usb_input_device_connected_;
  }
  virtual LidState QueryLidState() OVERRIDE {
    return lid_state_;
  }
  virtual bool IsOobeCompleted() OVERRIDE { return oobe_completed_; }
  virtual bool ShouldAvoidSuspendForHeadphoneJack() OVERRIDE {
    return avoid_suspend_for_headphones_;
  }
  virtual void DimScreen() OVERRIDE { AppendAction(kScreenDim); }
  virtual void UndimScreen() OVERRIDE { AppendAction(kScreenUndim); }
  virtual void TurnScreenOff() OVERRIDE { AppendAction(kScreenOff); }
  virtual void TurnScreenOn() OVERRIDE { AppendAction(kScreenOn); }
  virtual void LockScreen() OVERRIDE { AppendAction(kScreenLock); }
  virtual void Suspend() OVERRIDE { AppendAction(kSuspend); }
  virtual void StopSession() OVERRIDE { AppendAction(kStopSession); }
  virtual void ShutDown() OVERRIDE { AppendAction(kShutDown); }
  virtual void EmitIdleNotification(base::TimeDelta delay) OVERRIDE {
    AppendAction(GetEmitIdleNotificationAction(delay));
  }
  virtual void ReportUserActivityMetrics() OVERRIDE {
    if (record_metrics_actions_)
      AppendAction(kReportUserActivityMetrics);
  }

 private:
  void AppendAction(const std::string& action) {
    if (!actions_.empty())
      actions_ += ",";
    actions_ += action;
  }

  // Should calls to ReportUserActivityMetrics() be recorded in |actions_|?
  // These are noisy, so by default, they aren't recorded.
  bool record_metrics_actions_;

  // Should IsUsbInputDeviceConnected() return true?
  bool usb_input_device_connected_;

  // Should IsOobeCompleted() return true?
  bool oobe_completed_;

  // Should ShouldAvoidSuspendForHeadphoneJack() return true?
  bool avoid_suspend_for_headphones_;

  // Lid state to be returned by QueryLidState().
  LidState lid_state_;

  std::string actions_;

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

}  // namespace

class StateControllerTest : public testing::Test {
 public:
  StateControllerTest()
      : controller_(&delegate_, &prefs_),
        test_api_(&controller_),
        now_(base::TimeTicks::FromInternalValue(1000)),
        default_ac_suspend_delay_(base::TimeDelta::FromSeconds(120)),
        default_ac_screen_off_delay_(base::TimeDelta::FromSeconds(100)),
        default_ac_screen_dim_delay_(base::TimeDelta::FromSeconds(90)),
        default_battery_suspend_delay_(base::TimeDelta::FromSeconds(60)),
        default_battery_screen_off_delay_(base::TimeDelta::FromSeconds(40)),
        default_battery_screen_dim_delay_(base::TimeDelta::FromSeconds(30)),
        default_disable_idle_suspend_(0),
        default_require_usb_input_device_to_suspend_(0),
        default_keep_screen_on_for_audio_(0),
        default_has_lid_(1),
        initial_power_source_(POWER_AC),
        initial_lid_state_(LID_OPEN),
        initial_session_state_(SESSION_STARTED),
        initial_display_mode_(DISPLAY_NORMAL) {
  }

 protected:
  void SetMillisecondPref(const std::string& name, base::TimeDelta value) {
    CHECK(prefs_.SetInt64(name, value.InMilliseconds()));
  }

  // Sets values in |prefs_| based on |default_*| members and initializes
  // |controller_|.
  void Init() {
    SetMillisecondPref(kPluggedSuspendMsPref, default_ac_suspend_delay_);
    SetMillisecondPref(kPluggedOffMsPref, default_ac_screen_off_delay_);
    SetMillisecondPref(kPluggedDimMsPref, default_ac_screen_dim_delay_);
    SetMillisecondPref(kUnpluggedSuspendMsPref, default_battery_suspend_delay_);
    SetMillisecondPref(kUnpluggedOffMsPref, default_battery_screen_off_delay_);
    SetMillisecondPref(kUnpluggedDimMsPref, default_battery_screen_dim_delay_);
    CHECK(prefs_.SetInt64(kDisableIdleSuspendPref,
                          default_disable_idle_suspend_));
    CHECK(prefs_.SetInt64(kRequireUsbInputDeviceToSuspendPref,
                          default_require_usb_input_device_to_suspend_));
    CHECK(prefs_.SetInt64(kKeepBacklightOnForAudioPref,
                          default_keep_screen_on_for_audio_));
    CHECK(prefs_.SetInt64(kUseLidPref, default_has_lid_));

    test_api_.SetCurrentTime(now_);
    controller_.Init(initial_power_source_, initial_lid_state_,
                     initial_session_state_, initial_display_mode_);
  }

  // Advances |now_| by |interval_|.
  void AdvanceTime(base::TimeDelta interval) {
    now_ += interval;
    test_api_.SetCurrentTime(now_);
  }

  // Checks that |controller_|'s timeout is scheduled for |now_| and then
  // runs it.  Returns false if the timeout isn't scheduled or is scheduled
  // for a different time.
  bool TriggerTimeout() {
    base::TimeTicks timeout_time = test_api_.GetTimeoutTime();
    if (timeout_time == base::TimeTicks()) {
      LOG(ERROR) << "Ignoring request to trigger unscheduled timeout at "
                 << now_.ToInternalValue();
      return false;
    }
    if (timeout_time != now_) {
      LOG(ERROR) << "Ignoring request to trigger timeout scheduled for "
                 << timeout_time.ToInternalValue() << " at "
                 << now_.ToInternalValue();
      return false;
    }
    test_api_.TriggerTimeout();
    return true;
  }

  // Advances |now_| by |interval| and calls TriggerTimeout().
  bool AdvanceTimeAndTriggerTimeout(base::TimeDelta interval) {
    AdvanceTime(interval);
    return TriggerTimeout();
  }

  // Advances |now_| by |next_delay| minus the last delay passed to this
  // method and calls TriggerTimeout().  This is useful when invoking
  // successive delays: for example, given delays at 2, 4, and 5 minutes,
  // instead of calling AdvanceTimeAndTriggerTimeout() with 2, (4 - 2), and
  // then (5 - 4), StepTimeAndTriggerTimeout() can be called with 2, 4, and
  // 5.  Call ResetLastStepDelay() before a new sequence of delays to reset
  // the "last delay".
  bool StepTimeAndTriggerTimeout(base::TimeDelta next_delay) {
    AdvanceTime(next_delay - last_step_delay_);
    last_step_delay_ = next_delay;
    return TriggerTimeout();
  }

  // Resets the "last delay" used by StepTimeAndTriggerTimeout().
  void ResetLastStepDelay() { last_step_delay_ = base::TimeDelta(); }

  // Steps through time to trigger the default AC screen dim, off, and
  // suspend timeouts.
  void TriggerDefaultAcTimeouts() {
    ResetLastStepDelay();
    ASSERT_TRUE(StepTimeAndTriggerTimeout(default_ac_screen_dim_delay_));
    ASSERT_TRUE(StepTimeAndTriggerTimeout(default_ac_screen_off_delay_));
    ASSERT_TRUE(StepTimeAndTriggerTimeout(default_ac_suspend_delay_));
  }

  FakePrefs prefs_;
  TestDelegate delegate_;
  StateController controller_;
  StateController::TestApi test_api_;

  base::TimeTicks now_;

  // Last delay that was passed to StepTimeAndTriggerTimeout().
  base::TimeDelta last_step_delay_;

  // Preference values.  Tests may change these before calling Init().
  base::TimeDelta default_ac_suspend_delay_;
  base::TimeDelta default_ac_screen_off_delay_;
  base::TimeDelta default_ac_screen_dim_delay_;
  base::TimeDelta default_battery_suspend_delay_;
  base::TimeDelta default_battery_screen_off_delay_;
  base::TimeDelta default_battery_screen_dim_delay_;
  int64 default_disable_idle_suspend_;
  int64 default_require_usb_input_device_to_suspend_;
  int64 default_keep_screen_on_for_audio_;
  int64 default_has_lid_;

  // Values passed by Init() to StateController::Init().
  PowerSource initial_power_source_;
  LidState initial_lid_state_;
  SessionState initial_session_state_;
  DisplayMode initial_display_mode_;
};

// Tests the basic operation of the different delays.
TEST_F(StateControllerTest, BasicDelays) {
  Init();

  // The screen should be dimmed after the configured interval and then undimmed
  // in response to user activity.
  ASSERT_TRUE(AdvanceTimeAndTriggerTimeout(default_ac_screen_dim_delay_));
  EXPECT_EQ(kScreenDim, delegate_.GetActions());
  controller_.HandleUserActivity();
  EXPECT_EQ(kScreenUndim, delegate_.GetActions());

  // The system should eventually suspend if the user is inactive.
  ASSERT_TRUE(StepTimeAndTriggerTimeout(default_ac_screen_dim_delay_));
  EXPECT_EQ(kScreenDim, delegate_.GetActions());
  ASSERT_TRUE(StepTimeAndTriggerTimeout(default_ac_screen_off_delay_));
  EXPECT_EQ(kScreenOff, delegate_.GetActions());
  ASSERT_TRUE(StepTimeAndTriggerTimeout(default_ac_suspend_delay_));
  EXPECT_EQ(kSuspend, delegate_.GetActions());

  // No further timeouts should be scheduled at this point.
  EXPECT_TRUE(test_api_.GetTimeoutTime().is_null());

  // When the system resumes, the screen should be undimmed and turned back
  // on.
  controller_.HandleResume();
  EXPECT_EQ(JoinActions(kScreenUndim, kScreenOn, NULL), delegate_.GetActions());

  // The screen should be dimmed again after the screen-dim delay.
  ASSERT_TRUE(AdvanceTimeAndTriggerTimeout(default_ac_screen_dim_delay_));
  EXPECT_EQ(kScreenDim, delegate_.GetActions());
}

// Tests that the screen isn't dimmed while video is detected.
TEST_F(StateControllerTest, VideoDefersDimming) {
  Init();

  // The screen shouldn't be dimmed while a video is playing.
  const base::TimeDelta kHalfDimDelay = default_ac_screen_dim_delay_ / 2;
  controller_.HandleVideoActivity();
  AdvanceTime(kHalfDimDelay);
  controller_.HandleVideoActivity();
  AdvanceTime(kHalfDimDelay);
  controller_.HandleVideoActivity();
  AdvanceTime(kHalfDimDelay);
  controller_.HandleVideoActivity();
  EXPECT_EQ(kNoActions, delegate_.GetActions());

  // After the video stops, the dimming delay should happen as expected.
  ASSERT_TRUE(AdvanceTimeAndTriggerTimeout(default_ac_screen_dim_delay_));
  EXPECT_EQ(kScreenDim, delegate_.GetActions());

  // Video activity should undim the screen at this point.
  controller_.HandleVideoActivity();
  EXPECT_EQ(kScreenUndim, delegate_.GetActions());

  // The dimming delay should fire again after the video stops.
  ASSERT_TRUE(AdvanceTimeAndTriggerTimeout(default_ac_screen_dim_delay_));
  EXPECT_EQ(kScreenDim, delegate_.GetActions());
}

// Tests that the screen dims, is turned off, and is locked while audio is
// playing.
TEST_F(StateControllerTest, AudioDefersSuspend) {
  Init();

  const base::TimeDelta kDimDelay = base::TimeDelta::FromSeconds(300);
  const base::TimeDelta kOffDelay = base::TimeDelta::FromSeconds(310);
  const base::TimeDelta kLockDelay = base::TimeDelta::FromSeconds(320);
  const base::TimeDelta kIdleDelay = base::TimeDelta::FromSeconds(330);

  PowerManagementPolicy policy;
  policy.mutable_ac_delays()->set_screen_dim_ms(kDimDelay.InMilliseconds());
  policy.mutable_ac_delays()->set_screen_off_ms(kOffDelay.InMilliseconds());
  policy.mutable_ac_delays()->set_screen_lock_ms(kLockDelay.InMilliseconds());
  policy.mutable_ac_delays()->set_idle_ms(kIdleDelay.InMilliseconds());
  controller_.HandlePolicyChange(policy);

  // Report audio activity and check that the controller goes through the
  // usual dim->off->lock progression.
  controller_.HandleAudioActivity();
  ASSERT_TRUE(StepTimeAndTriggerTimeout(kDimDelay));
  EXPECT_EQ(kScreenDim, delegate_.GetActions());
  ASSERT_TRUE(StepTimeAndTriggerTimeout(kOffDelay));
  EXPECT_EQ(kScreenOff, delegate_.GetActions());
  ASSERT_TRUE(StepTimeAndTriggerTimeout(kLockDelay));
  EXPECT_EQ(kScreenLock, delegate_.GetActions());

  // Report additional audio activity.  The controller should wait for the
  // full suspend delay before suspending.
  controller_.HandleAudioActivity();
  EXPECT_EQ(kNoActions, delegate_.GetActions());
  ASSERT_TRUE(AdvanceTimeAndTriggerTimeout(kIdleDelay));
  EXPECT_EQ(kSuspend, delegate_.GetActions());
}

// Tests that the system is suspended when the lid is closed.
TEST_F(StateControllerTest, LidCloseSuspendsByDefault) {
  Init();
  controller_.HandleLidStateChange(LID_CLOSED);
  EXPECT_EQ(kSuspend, delegate_.GetActions());

  // After the lid is opened, the next delay should be screen-dimming (i.e.
  // all timers should be reset).
  delegate_.set_lid_state(LID_OPEN);
  controller_.HandleResume();
  EXPECT_EQ(kNoActions, delegate_.GetActions());
  controller_.HandleLidStateChange(LID_OPEN);
  EXPECT_EQ(kNoActions, delegate_.GetActions());
  ASSERT_TRUE(AdvanceTimeAndTriggerTimeout(default_ac_screen_dim_delay_));
  EXPECT_EQ(kScreenDim, delegate_.GetActions());
}

// Tests that timeouts are reset when the user logs in or out.
TEST_F(StateControllerTest, SessionStateChangeResetsTimeouts) {
  Init();
  ASSERT_TRUE(StepTimeAndTriggerTimeout(default_ac_screen_dim_delay_));
  ASSERT_TRUE(StepTimeAndTriggerTimeout(default_ac_screen_off_delay_));
  EXPECT_EQ(JoinActions(kScreenDim, kScreenOff, NULL), delegate_.GetActions());

  // The screen should be undimmed and turned on when a user logs out.
  controller_.HandleSessionStateChange(SESSION_STOPPED);
  EXPECT_EQ(JoinActions(kScreenUndim, kScreenOn, NULL), delegate_.GetActions());

  // The screen should be dimmed again after the usual delay.
  ASSERT_TRUE(AdvanceTimeAndTriggerTimeout(default_ac_screen_dim_delay_));
  EXPECT_EQ(kScreenDim, delegate_.GetActions());
}

// Tests the controller shuts the system down instead of suspending when no
// user is logged in.
TEST_F(StateControllerTest, ShutDownWhenSessionStopped) {
  initial_session_state_ = SESSION_STOPPED;
  Init();

  // The screen should be dimmed and turned off, but the system should shut
  // down instead of suspending.
  TriggerDefaultAcTimeouts();
  EXPECT_EQ(JoinActions(kScreenDim, kScreenOff, kShutDown, NULL),
            delegate_.GetActions());

  // Send a session-started notification (which is a bit unrealistic given
  // that the system was just shut down).
  controller_.HandleSessionStateChange(SESSION_STARTED);
  EXPECT_EQ(JoinActions(kScreenUndim, kScreenOn, NULL), delegate_.GetActions());

  // The system should suspend now.
  TriggerDefaultAcTimeouts();
  EXPECT_EQ(JoinActions(kScreenDim, kScreenOff, kSuspend, NULL),
            delegate_.GetActions());

  // After resuming and stopping the session, lid-close should shut the
  // system down.
  controller_.HandleResume();
  EXPECT_EQ(JoinActions(kScreenUndim, kScreenOn, NULL), delegate_.GetActions());
  controller_.HandleSessionStateChange(SESSION_STOPPED);
  EXPECT_EQ(kNoActions, delegate_.GetActions());
  controller_.HandleLidStateChange(LID_CLOSED);
  EXPECT_EQ(kShutDown, delegate_.GetActions());
}

// Tests that delays are scaled while presenting and that they return to
// their original values when not presenting.
TEST_F(StateControllerTest, ScaleDelaysWhilePresenting) {
  initial_display_mode_ = DISPLAY_PRESENTATION;
  Init();

  // The suspend delay should be scaled; all others should be updated to
  // retain the same difference from the suspend delay as before.
  base::TimeDelta suspend_delay = default_ac_suspend_delay_ *
      StateController::kDefaultPresentationIdleDelayFactor;
  base::TimeDelta screen_off_delay = suspend_delay -
      (default_ac_suspend_delay_ - default_ac_screen_off_delay_);
  base::TimeDelta screen_dim_delay = suspend_delay -
      (default_ac_suspend_delay_ - default_ac_screen_dim_delay_);

  ASSERT_TRUE(StepTimeAndTriggerTimeout(screen_dim_delay));
  EXPECT_EQ(kScreenDim, delegate_.GetActions());
  ASSERT_TRUE(StepTimeAndTriggerTimeout(screen_off_delay));
  EXPECT_EQ(kScreenOff, delegate_.GetActions());
  ASSERT_TRUE(StepTimeAndTriggerTimeout(suspend_delay));
  EXPECT_EQ(kSuspend, delegate_.GetActions());

  controller_.HandleResume();
  EXPECT_EQ(JoinActions(kScreenUndim, kScreenOn, NULL), delegate_.GetActions());
  controller_.HandleDisplayModeChange(DISPLAY_NORMAL);
  EXPECT_EQ(kNoActions, delegate_.GetActions());
  ResetLastStepDelay();
  ASSERT_TRUE(StepTimeAndTriggerTimeout(default_ac_screen_dim_delay_));
  EXPECT_EQ(kScreenDim, delegate_.GetActions());
  ASSERT_TRUE(StepTimeAndTriggerTimeout(default_ac_screen_off_delay_));
  EXPECT_EQ(kScreenOff, delegate_.GetActions());
  ASSERT_TRUE(StepTimeAndTriggerTimeout(default_ac_suspend_delay_));
  EXPECT_EQ(kSuspend, delegate_.GetActions());
}

// Tests that the appropriate delays are used when switching between battery
// and AC power.
TEST_F(StateControllerTest, PowerSourceChange) {
  // Start out on battery power.
  initial_power_source_ = POWER_BATTERY;
  default_battery_screen_dim_delay_ = base::TimeDelta::FromSeconds(60);
  default_battery_screen_off_delay_ = base::TimeDelta::FromSeconds(90);
  default_battery_suspend_delay_ = base::TimeDelta::FromSeconds(100);
  default_ac_screen_dim_delay_ = base::TimeDelta::FromSeconds(120);
  default_ac_screen_off_delay_ = base::TimeDelta::FromSeconds(150);
  default_ac_suspend_delay_ = base::TimeDelta::FromSeconds(160);
  Init();

  ASSERT_TRUE(StepTimeAndTriggerTimeout(default_battery_screen_dim_delay_));
  EXPECT_EQ(kScreenDim, delegate_.GetActions());
  ASSERT_TRUE(StepTimeAndTriggerTimeout(default_battery_screen_off_delay_));
  EXPECT_EQ(kScreenOff, delegate_.GetActions());
  ASSERT_TRUE(StepTimeAndTriggerTimeout(default_battery_suspend_delay_));
  EXPECT_EQ(kSuspend, delegate_.GetActions());

  // Switch to AC power and check that the AC delays are used instead.
  controller_.HandleResume();
  EXPECT_EQ(JoinActions(kScreenUndim, kScreenOn, NULL), delegate_.GetActions());
  controller_.HandlePowerSourceChange(POWER_AC);
  ResetLastStepDelay();
  ASSERT_TRUE(StepTimeAndTriggerTimeout(default_ac_screen_dim_delay_));
  EXPECT_EQ(kScreenDim, delegate_.GetActions());
  ASSERT_TRUE(StepTimeAndTriggerTimeout(default_ac_screen_off_delay_));
  EXPECT_EQ(kScreenOff, delegate_.GetActions());
  ASSERT_TRUE(StepTimeAndTriggerTimeout(default_ac_suspend_delay_));
  EXPECT_EQ(kSuspend, delegate_.GetActions());

  // Resume and wait for the screen to be dimmed.
  controller_.HandleResume();
  EXPECT_EQ(JoinActions(kScreenUndim, kScreenOn, NULL), delegate_.GetActions());
  ASSERT_TRUE(AdvanceTimeAndTriggerTimeout(default_ac_screen_dim_delay_));
  EXPECT_EQ(kScreenDim, delegate_.GetActions());

  // Switch back to battery.  The controller should treat the power source
  // change as a user action and undim the screen (rather than e.g.
  // suspending immediately since |default_battery_suspend_delay_| has been
  // exceeded) and then proceed through the battery delays.
  controller_.HandlePowerSourceChange(POWER_BATTERY);
  EXPECT_EQ(kScreenUndim, delegate_.GetActions());
  ResetLastStepDelay();
  ASSERT_TRUE(StepTimeAndTriggerTimeout(default_battery_screen_dim_delay_));
  EXPECT_EQ(kScreenDim, delegate_.GetActions());
  ASSERT_TRUE(StepTimeAndTriggerTimeout(default_battery_screen_off_delay_));
  EXPECT_EQ(kScreenOff, delegate_.GetActions());
  ASSERT_TRUE(StepTimeAndTriggerTimeout(default_battery_suspend_delay_));
  EXPECT_EQ(kSuspend, delegate_.GetActions());
}

// Tests that externally-supplied policy supercedes powerd's default prefs.
TEST_F(StateControllerTest, PolicySupercedesPrefs) {
  Init();

  // Set an external policy that disables most delays and instructs the
  // power manager to log the user out after 10 minutes of inactivity.
  const base::TimeDelta kIdleDelay = base::TimeDelta::FromSeconds(600);
  PowerManagementPolicy policy;
  policy.mutable_ac_delays()->set_idle_ms(kIdleDelay.InMilliseconds());
  policy.mutable_ac_delays()->set_screen_off_ms(0);
  policy.mutable_ac_delays()->set_screen_dim_ms(0);
  policy.mutable_ac_delays()->set_screen_lock_ms(0);
  *policy.mutable_battery_delays() = policy.ac_delays();
  policy.set_idle_action(PowerManagementPolicy_Action_STOP_SESSION);
  policy.set_lid_closed_action(PowerManagementPolicy_Action_DO_NOTHING);
  policy.set_use_audio_activity(false);
  policy.set_use_video_activity(false);
  policy.set_presentation_idle_delay_factor(1.0);
  controller_.HandlePolicyChange(policy);

  ASSERT_TRUE(AdvanceTimeAndTriggerTimeout(kIdleDelay));
  EXPECT_EQ(kStopSession, delegate_.GetActions());

  controller_.HandleUserActivity();
  controller_.HandleDisplayModeChange(DISPLAY_PRESENTATION);
  EXPECT_EQ(kNoActions, delegate_.GetActions());

  // Wait for half of the idle delay and then report user activity, which
  // should reset the logout timeout.  Audio and video activity should not
  // reset the timeout, however.
  AdvanceTime(kIdleDelay / 2);
  controller_.HandleUserActivity();
  AdvanceTime(kIdleDelay / 2);
  controller_.HandleAudioActivity();
  controller_.HandleVideoActivity();
  ASSERT_TRUE(AdvanceTimeAndTriggerTimeout(kIdleDelay / 2));
  EXPECT_EQ(kStopSession, delegate_.GetActions());

  // The policy's request to do nothing when the lid is closed should be
  // honored.
  controller_.HandleLidStateChange(LID_CLOSED);
  EXPECT_EQ(kNoActions, delegate_.GetActions());

  // Wait 120 seconds and then send an updated policy that dims the screen
  // after 60 seconds.  The screen should dim immediately.
  AdvanceTime(base::TimeDelta::FromSeconds(120));
  policy.mutable_ac_delays()->set_screen_dim_ms(60000);
  controller_.HandlePolicyChange(policy);
  EXPECT_EQ(kScreenDim, delegate_.GetActions());

  // Switch to battery power, which still has an unset screen-dimming
  // delay.  The screen should undim immediately.
  controller_.HandlePowerSourceChange(POWER_BATTERY);
  EXPECT_EQ(kScreenUndim, delegate_.GetActions());

  // Update the policy again to shut down if the lid is closed.  Since the
  // lid is already closed, the system should shut down immediately.
  policy.set_lid_closed_action(PowerManagementPolicy_Action_SHUT_DOWN);
  controller_.HandlePolicyChange(policy);
  EXPECT_EQ(kShutDown, delegate_.GetActions());
}

// Test that unset fields in a policy are ignored.
TEST_F(StateControllerTest, PartiallyFilledPolicy) {
  Init();

  // Set a policy that has a very short dimming delay but leaves all other
  // fields unset.
  const base::TimeDelta kDimDelay = base::TimeDelta::FromSeconds(1);
  PowerManagementPolicy policy;
  policy.mutable_ac_delays()->set_screen_dim_ms(kDimDelay.InMilliseconds());
  controller_.HandlePolicyChange(policy);

  // The policy's dimming delay should be used, but the rest of the delays
  // should come from prefs.
  ASSERT_TRUE(StepTimeAndTriggerTimeout(kDimDelay));
  EXPECT_EQ(kScreenDim, delegate_.GetActions());
  ASSERT_TRUE(StepTimeAndTriggerTimeout(default_ac_screen_off_delay_));
  EXPECT_EQ(kScreenOff, delegate_.GetActions());
  ASSERT_TRUE(StepTimeAndTriggerTimeout(default_ac_suspend_delay_));
  EXPECT_EQ(kSuspend, delegate_.GetActions());
  controller_.HandleResume();
  EXPECT_EQ(JoinActions(kScreenUndim, kScreenOn, NULL), delegate_.GetActions());

  // Setting an empty policy should revert to the values from the prefs.
  policy.Clear();
  controller_.HandlePolicyChange(policy);
  TriggerDefaultAcTimeouts();
  EXPECT_EQ(JoinActions(kScreenDim, kScreenOff, kSuspend, NULL),
            delegate_.GetActions());
}

// Tests that policies that enable audio detection while disabling video
// detection result in the screen getting locked at the expected time but
// defer suspend.
TEST_F(StateControllerTest, PolicyDisablingVideo) {
  Init();

  const base::TimeDelta kDimDelay = base::TimeDelta::FromSeconds(300);
  const base::TimeDelta kOffDelay = base::TimeDelta::FromSeconds(310);
  const base::TimeDelta kLockDelay = base::TimeDelta::FromSeconds(320);
  const base::TimeDelta kIdleDelay = base::TimeDelta::FromSeconds(330);

  PowerManagementPolicy policy;
  policy.mutable_ac_delays()->set_screen_dim_ms(kDimDelay.InMilliseconds());
  policy.mutable_ac_delays()->set_screen_off_ms(kOffDelay.InMilliseconds());
  policy.mutable_ac_delays()->set_screen_lock_ms(kLockDelay.InMilliseconds());
  policy.mutable_ac_delays()->set_idle_ms(kIdleDelay.InMilliseconds());
  policy.set_idle_action(PowerManagementPolicy_Action_SUSPEND);
  policy.set_use_audio_activity(true);
  policy.set_use_video_activity(false);
  controller_.HandlePolicyChange(policy);

  // Proceed through the screen-dim, screen-off, and screen-lock delays,
  // reporting video and audio activity along the way.  The screen should
  // be locked (since |use_video_activity| is false).
  controller_.HandleVideoActivity();
  controller_.HandleAudioActivity();
  EXPECT_EQ(kNoActions, delegate_.GetActions());
  ASSERT_TRUE(StepTimeAndTriggerTimeout(kDimDelay));
  EXPECT_EQ(kScreenDim, delegate_.GetActions());
  controller_.HandleVideoActivity();
  controller_.HandleAudioActivity();
  EXPECT_EQ(kNoActions, delegate_.GetActions());
  ASSERT_TRUE(StepTimeAndTriggerTimeout(kOffDelay));
  EXPECT_EQ(kScreenOff, delegate_.GetActions());
  controller_.HandleVideoActivity();
  controller_.HandleAudioActivity();
  EXPECT_EQ(kNoActions, delegate_.GetActions());
  ASSERT_TRUE(StepTimeAndTriggerTimeout(kLockDelay));
  EXPECT_EQ(kScreenLock, delegate_.GetActions());

  // The system shouldn't suspend until a full |kIdleDelay| after the last
  // report of audio activity, since |use_audio_activity| is false.
  controller_.HandleVideoActivity();
  controller_.HandleAudioActivity();
  EXPECT_EQ(kNoActions, delegate_.GetActions());
  ASSERT_TRUE(AdvanceTimeAndTriggerTimeout(kIdleDelay));
  EXPECT_EQ(kSuspend, delegate_.GetActions());
}

// Tests that the controller does something reasonable if the lid is closed
// just as the idle delay is reached but before the timeout has fired.
TEST_F(StateControllerTest, SimultaneousIdleAndLidActions) {
  Init();

  // Step through the normal delays.  Just when the suspend delay is about
  // to run, close the lid.  We should only make one suspend attempt.
  ASSERT_TRUE(StepTimeAndTriggerTimeout(default_ac_screen_dim_delay_));
  ASSERT_TRUE(StepTimeAndTriggerTimeout(default_ac_screen_off_delay_));
  EXPECT_EQ(JoinActions(kScreenDim, kScreenOff, NULL), delegate_.GetActions());
  AdvanceTime(default_ac_suspend_delay_ - default_ac_screen_off_delay_);
  controller_.HandleLidStateChange(LID_CLOSED);
  EXPECT_EQ(kSuspend, delegate_.GetActions());
}

// Tests that the screen stays on while audio is playing if
// |kKeepBacklightOnForAudioPref| is set.
TEST_F(StateControllerTest, KeepScreenOnForAudio) {
  default_keep_screen_on_for_audio_ = 1;
  Init();
  const base::TimeDelta kHalfScreenOffDelay = default_ac_screen_off_delay_ / 2;

  // The screen should be dimmed as usual.
  ASSERT_TRUE(StepTimeAndTriggerTimeout(default_ac_screen_dim_delay_));
  EXPECT_EQ(kScreenDim, delegate_.GetActions());

  // After audio is reported, screen-off should be deferred.
  controller_.HandleAudioActivity();
  EXPECT_EQ(kNoActions, delegate_.GetActions());

  // Continue reporting audio activity; the screen should stay on.
  controller_.HandleAudioActivity();
  EXPECT_EQ(kNoActions, delegate_.GetActions());
  AdvanceTime(kHalfScreenOffDelay);
  controller_.HandleAudioActivity();
  EXPECT_EQ(kNoActions, delegate_.GetActions());
  AdvanceTime(kHalfScreenOffDelay);
  controller_.HandleAudioActivity();
  EXPECT_EQ(kNoActions, delegate_.GetActions());

  // After the audio activity stops, the screen should be turned off after
  // the normal screen-off delay.
  ASSERT_TRUE(AdvanceTimeAndTriggerTimeout(default_ac_screen_off_delay_));
  EXPECT_EQ(kScreenOff, delegate_.GetActions());

  // Audio activity should turn the screen back on.
  controller_.HandleAudioActivity();
  EXPECT_EQ(kScreenOn, delegate_.GetActions());

  // Turn the screen off again and check that the next action is suspending.
  ResetLastStepDelay();
  ASSERT_TRUE(StepTimeAndTriggerTimeout(default_ac_screen_off_delay_));
  EXPECT_EQ(kScreenOff, delegate_.GetActions());
  ASSERT_TRUE(StepTimeAndTriggerTimeout(default_ac_suspend_delay_));
  EXPECT_EQ(kSuspend, delegate_.GetActions());
}

// Tests that the |kRequireUsbInputDeviceToSuspendPref| pref is honored.
TEST_F(StateControllerTest, RequireUsbInputDeviceToSuspend) {
  default_require_usb_input_device_to_suspend_ = 1;
  delegate_.set_usb_input_device_connected(false);
  Init();

  // Advance through the usual delays.  The suspend timeout should
  // trigger as before, but no action should be performed.
  TriggerDefaultAcTimeouts();
  EXPECT_EQ(JoinActions(kScreenDim, kScreenOff, NULL), delegate_.GetActions());

  // After a USB input device is connected, the system should suspend as
  // before.
  delegate_.set_usb_input_device_connected(true);
  controller_.HandleUserActivity();
  EXPECT_EQ(JoinActions(kScreenUndim, kScreenOn, NULL), delegate_.GetActions());
  TriggerDefaultAcTimeouts();
  EXPECT_EQ(JoinActions(kScreenDim, kScreenOff, kSuspend, NULL),
            delegate_.GetActions());
}

// Tests that suspend is deferred before OOBE is completed.
TEST_F(StateControllerTest, DontSuspendBeforeOobeCompleted) {
  delegate_.set_oobe_completed(false);
  Init();

  // The screen should dim and turn off as usual, but the system shouldn't
  // be suspended.
  TriggerDefaultAcTimeouts();
  EXPECT_EQ(JoinActions(kScreenDim, kScreenOff, NULL), delegate_.GetActions());

  // Report user activity and mark OOBE as done.  The system should suspend
  // this time.
  controller_.HandleUserActivity();
  EXPECT_EQ(JoinActions(kScreenUndim, kScreenOn, NULL), delegate_.GetActions());
  delegate_.set_oobe_completed(true);
  TriggerDefaultAcTimeouts();
  EXPECT_EQ(JoinActions(kScreenDim, kScreenOff, kSuspend, NULL),
            delegate_.GetActions());
}

// Tests that the disable-idle-suspend pref is honored and overrides policies.
TEST_F(StateControllerTest, DisableIdleSuspend) {
  default_disable_idle_suspend_ = 1;
  Init();

  // With the disable-idle-suspend pref set, the system shouldn't suspend
  // when it's idle.
  TriggerDefaultAcTimeouts();
  EXPECT_EQ(JoinActions(kScreenDim, kScreenOff, NULL), delegate_.GetActions());

  // Even after explicitly setting a policy to suspend on idle, the system
  // should still stay up.
  PowerManagementPolicy policy;
  policy.set_idle_action(PowerManagementPolicy_Action_SUSPEND);
  controller_.HandlePolicyChange(policy);
  EXPECT_EQ(kNoActions, delegate_.GetActions());

  // The pref should also override the shutdown-on-idle action that's the
  // default when the session is stopped.
  controller_.HandlePolicyChange(PowerManagementPolicy());
  controller_.HandleSessionStateChange(SESSION_STOPPED);
  EXPECT_EQ(JoinActions(kScreenUndim, kScreenOn, NULL), delegate_.GetActions());
  TriggerDefaultAcTimeouts();
  EXPECT_EQ(JoinActions(kScreenDim, kScreenOff, NULL), delegate_.GetActions());

  // The controller should watch the pref for changes.  After setting it to
  // 0, the system should shut down due to inactivity.
  controller_.HandleUserActivity();
  EXPECT_EQ(JoinActions(kScreenUndim, kScreenOn, NULL), delegate_.GetActions());
  ASSERT_TRUE(prefs_.SetInt64(kDisableIdleSuspendPref, 0));
  prefs_.NotifyObservers(kDisableIdleSuspendPref);
  TriggerDefaultAcTimeouts();
  EXPECT_EQ(JoinActions(kScreenDim, kScreenOff, kShutDown, NULL),
            delegate_.GetActions());
}

// Tests that state overrides are honored.
TEST_F(StateControllerTest, Overrides) {
  Init();

  // Override everything.  The idle timeout should fire but do nothing.
  controller_.HandleOverrideChange(true /* override_screen_dim */,
                                   true /* override_screen_off */,
                                   true /* override_idle_suspend */,
                                   true /* override_lid_suspend */);
  ASSERT_TRUE(AdvanceTimeAndTriggerTimeout(default_ac_suspend_delay_));
  EXPECT_EQ(kNoActions, delegate_.GetActions());
  controller_.HandleLidStateChange(LID_CLOSED);
  EXPECT_EQ(kNoActions, delegate_.GetActions());
  controller_.HandleLidStateChange(LID_OPEN);

  // Override the suspend properties but not the screen-related delays and
  // check that the controller dims and turns off the screen but doesn't
  // suspend the system.
  controller_.HandleOverrideChange(false /* override_screen_dim */,
                                   false /* override_screen_off */,
                                   true /* override_idle_suspend */,
                                   true /* override_lid_suspend */);
  ASSERT_TRUE(StepTimeAndTriggerTimeout(default_ac_screen_dim_delay_));
  EXPECT_EQ(kScreenDim, delegate_.GetActions());
  ASSERT_TRUE(StepTimeAndTriggerTimeout(default_ac_screen_off_delay_));
  EXPECT_EQ(kScreenOff, delegate_.GetActions());
  ASSERT_TRUE(StepTimeAndTriggerTimeout(default_ac_suspend_delay_));
  EXPECT_EQ(kNoActions, delegate_.GetActions());
  controller_.HandleLidStateChange(LID_CLOSED);
  EXPECT_EQ(kNoActions, delegate_.GetActions());

  // If the lid override is removed while the lid is still closed, the
  // system should suspend immediately.
  controller_.HandleOverrideChange(false /* override_screen_dim */,
                                   false /* override_screen_off */,
                                   true /* override_idle_suspend */,
                                   false /* override_lid_suspend */);
  EXPECT_EQ(kSuspend, delegate_.GetActions());
}

// Tests that the controller does something reasonable when given delays
// that don't make sense.
TEST_F(StateControllerTest, InvalidDelays) {
  // The dim delay should be less than the off delay, which should be less
  // than the idle delay.  All of those constraints are violated here, so
  // all of the other delays should be capped to the idle delay.
  default_ac_screen_dim_delay_ = base::TimeDelta::FromSeconds(120);
  default_ac_screen_off_delay_ = base::TimeDelta::FromSeconds(110);
  default_ac_suspend_delay_ = base::TimeDelta::FromSeconds(100);
  Init();
  ASSERT_TRUE(AdvanceTimeAndTriggerTimeout(default_ac_suspend_delay_));
  EXPECT_EQ(JoinActions(kScreenDim, kScreenOff, kSuspend, NULL),
            delegate_.GetActions());

  // Policy delays should also be cleaned up.
  const base::TimeDelta kDimDelay = base::TimeDelta::FromSeconds(70);
  const base::TimeDelta kOffDelay = base::TimeDelta::FromSeconds(50);
  const base::TimeDelta kLockDelay = base::TimeDelta::FromSeconds(80);
  const base::TimeDelta kIdleDelay = base::TimeDelta::FromSeconds(60);

  PowerManagementPolicy policy;
  policy.mutable_ac_delays()->set_screen_dim_ms(kDimDelay.InMilliseconds());
  policy.mutable_ac_delays()->set_screen_off_ms(kOffDelay.InMilliseconds());
  policy.mutable_ac_delays()->set_screen_lock_ms(kLockDelay.InMilliseconds());
  policy.mutable_ac_delays()->set_idle_ms(kIdleDelay.InMilliseconds());
  controller_.HandlePolicyChange(policy);

  // The screen-dim delay should be capped to the screen-off delay, while
  // the screen-lock delay should be ignored since it extends beyond the
  // suspend delay.
  controller_.HandleResume();
  EXPECT_EQ(JoinActions(kScreenUndim, kScreenOn, NULL), delegate_.GetActions());
  ResetLastStepDelay();
  ASSERT_TRUE(StepTimeAndTriggerTimeout(kOffDelay));
  EXPECT_EQ(JoinActions(kScreenDim, kScreenOff, NULL), delegate_.GetActions());
  ASSERT_TRUE(StepTimeAndTriggerTimeout(kIdleDelay));
  EXPECT_EQ(kSuspend, delegate_.GetActions());
}

// Tests that idle notifications requested by outside processes are honored.
TEST_F(StateControllerTest, EmitIdleNotification) {
  // Clear the optional delays and make the suspend delay long enough that
  // it won't be hit
  default_ac_screen_dim_delay_ = base::TimeDelta();
  default_ac_screen_off_delay_ = base::TimeDelta();
  default_ac_suspend_delay_ = base::TimeDelta::FromSeconds(600);
  Init();

  // Add notifications at 40 and 50 seconds.  The 40-second one should be
  // the first to be sent.
  const base::TimeDelta kDelay40 = base::TimeDelta::FromSeconds(40);
  controller_.AddIdleNotification(kDelay40);
  const base::TimeDelta kDelay50 = base::TimeDelta::FromSeconds(50);
  controller_.AddIdleNotification(kDelay50);
  ASSERT_TRUE(AdvanceTimeAndTriggerTimeout(kDelay40));
  EXPECT_EQ(GetEmitIdleNotificationAction(kDelay40), delegate_.GetActions());

  // Add a notification at 30 seconds.  It should be sent immediately since
  // the user has been idle for 40 seconds.
  const base::TimeDelta kDelay30 = base::TimeDelta::FromSeconds(30);
  controller_.AddIdleNotification(kDelay30);
  EXPECT_EQ(GetEmitIdleNotificationAction(kDelay30), delegate_.GetActions());

  // 10 seconds later, the 50-second notification should be sent.
  ASSERT_TRUE(AdvanceTimeAndTriggerTimeout(kDelay50 - kDelay40));
  EXPECT_EQ(GetEmitIdleNotificationAction(kDelay50), delegate_.GetActions());

  // Add another 30-second notification.  If user activity is reported
  // halfway through it, it should be deferred for 30 more seconds.
  controller_.HandleUserActivity();
  controller_.AddIdleNotification(kDelay30);
  EXPECT_EQ(kNoActions, delegate_.GetActions());
  AdvanceTime(kDelay30 / 2);
  controller_.HandleUserActivity();
  ASSERT_TRUE(AdvanceTimeAndTriggerTimeout(kDelay30));
  EXPECT_EQ(GetEmitIdleNotificationAction(kDelay30), delegate_.GetActions());
}

// Tests that the controller cues the delegate to report metrics when user
// activity is observed.
TEST_F(StateControllerTest, ReportMetrics) {
  delegate_.set_record_metrics_actions(true);
  Init();

  // Various events considered to represent user activity (direct activity,
  // power source changes, presentation mode, etc.) should all trigger
  // metrics.
  controller_.HandleUserActivity();
  EXPECT_EQ(kReportUserActivityMetrics, delegate_.GetActions());
  ASSERT_TRUE(AdvanceTimeAndTriggerTimeout(default_ac_screen_dim_delay_));
  EXPECT_EQ(kScreenDim, delegate_.GetActions());
  controller_.HandlePowerSourceChange(POWER_BATTERY);
  EXPECT_EQ(JoinActions(kReportUserActivityMetrics, kScreenUndim, NULL),
            delegate_.GetActions());
  AdvanceTime(default_ac_screen_dim_delay_ / 2);
  controller_.HandleDisplayModeChange(DISPLAY_PRESENTATION);
  EXPECT_EQ(kReportUserActivityMetrics, delegate_.GetActions());
}

// Tests that we avoid suspending while headphones are connected when so
// requested.
TEST_F(StateControllerTest, AvoidSuspendForHeadphoneJack) {
  Init();

  // With headphones connected, we shouldn't suspend.
  delegate_.set_avoid_suspend_for_headphones(true);
  TriggerDefaultAcTimeouts();
  EXPECT_EQ(JoinActions(kScreenDim, kScreenOff, NULL), delegate_.GetActions());

  // Without headphones, we should.
  delegate_.set_avoid_suspend_for_headphones(false);
  controller_.HandleUserActivity();
  EXPECT_EQ(JoinActions(kScreenUndim, kScreenOn, NULL), delegate_.GetActions());
  TriggerDefaultAcTimeouts();
  EXPECT_EQ(JoinActions(kScreenDim, kScreenOff, kSuspend, NULL),
            delegate_.GetActions());

  // Non-suspend actions should still be performed while headphones are
  // connected.
  controller_.HandleResume();
  delegate_.set_avoid_suspend_for_headphones(true);
  EXPECT_EQ(JoinActions(kScreenUndim, kScreenOn, NULL), delegate_.GetActions());
  PowerManagementPolicy policy;
  policy.set_idle_action(PowerManagementPolicy_Action_SHUT_DOWN);
  controller_.HandlePolicyChange(policy);
  EXPECT_EQ(kNoActions, delegate_.GetActions());
  TriggerDefaultAcTimeouts();
  EXPECT_EQ(JoinActions(kScreenDim, kScreenOff, kShutDown, NULL),
            delegate_.GetActions());
}

// Tests that the controller handles being woken from idle-suspend by a
// lid-close event (http://crosbug.com/38011).
TEST_F(StateControllerTest, LidCloseAfterIdleSuspend) {
  Init();
  TriggerDefaultAcTimeouts();
  EXPECT_EQ(JoinActions(kScreenDim, kScreenOff, kSuspend, NULL),
            delegate_.GetActions());

  // Close the lid, which may wake the system.  The controller should
  // re-suspend immediately after it receives the lid-closed event, without
  // turning the screen back on.
  delegate_.set_lid_state(LID_CLOSED);
  controller_.HandleResume();
  EXPECT_EQ(kNoActions, delegate_.GetActions());
  controller_.HandleLidStateChange(LID_CLOSED);
  EXPECT_EQ(kSuspend, delegate_.GetActions());
}

// Tests that the controller resuspends after a resume from
// suspend-from-lid-closed if the lid is opened and closed so quickly that
// no events are generated (http://crosbug.com/p/17499).
TEST_F(StateControllerTest, ResuspendAfterLidOpenAndClose) {
  Init();
  delegate_.set_lid_state(LID_CLOSED);
  controller_.HandleLidStateChange(LID_CLOSED);
  EXPECT_EQ(kSuspend, delegate_.GetActions());

  // The lid-closed action should be repeated if the lid is still closed
  // when the system resumes.
  controller_.HandleResume();
  EXPECT_EQ(kSuspend, delegate_.GetActions());
}

// Tests that the lid action is ignored if the "use lid" pref is false.
TEST_F(StateControllerTest, IgnoreLidEventsIfNoLid) {
  default_has_lid_ = 0;
  Init();
  delegate_.set_lid_state(LID_CLOSED);
  controller_.HandleLidStateChange(LID_CLOSED);
  EXPECT_EQ(kNoActions, delegate_.GetActions());
}

// Tests that the system doesn't suspend while an update is being applied.
TEST_F(StateControllerTest, AvoidSuspendDuringSystemUpdate) {
  Init();

  // Inform the controller that an update is being applied.  The screen
  // should dim and be turned off, but the system should stay on.
  controller_.HandleUpdaterStateChange(UPDATER_UPDATING);
  TriggerDefaultAcTimeouts();
  EXPECT_EQ(JoinActions(kScreenDim, kScreenOff, NULL), delegate_.GetActions());

  // When the update has been applied, the system should suspend immediately.
  controller_.HandleUpdaterStateChange(UPDATER_UPDATED);
  EXPECT_EQ(kSuspend, delegate_.GetActions());

  // Resume the system and announce another update.
  controller_.HandleResume();
  controller_.HandleUserActivity();
  EXPECT_EQ(JoinActions(kScreenUndim, kScreenOn, NULL), delegate_.GetActions());
  controller_.HandleUpdaterStateChange(UPDATER_UPDATING);

  // Closing the lid should still suspend.
  controller_.HandleLidStateChange(LID_CLOSED);
  EXPECT_EQ(kSuspend, delegate_.GetActions());

  // Step through all of the timeouts again.
  controller_.HandleResume();
  controller_.HandleLidStateChange(LID_OPEN);
  controller_.HandleUserActivity();
  TriggerDefaultAcTimeouts();
  EXPECT_EQ(JoinActions(kScreenDim, kScreenOff, NULL), delegate_.GetActions());

  // The system should also suspend immediately after transitioning from
  // the "updating" state back to "idle" (i.e. the update was unsuccessful).
  controller_.HandleUpdaterStateChange(UPDATER_IDLE);
  EXPECT_EQ(kSuspend, delegate_.GetActions());
  controller_.HandleResume();
  controller_.HandleUserActivity();
  EXPECT_EQ(JoinActions(kScreenUndim, kScreenOn, NULL), delegate_.GetActions());

  // If the idle action is changed to log the user out instead of
  // suspending or shutting down, it should still be performed while an
  // update is in-progress.
  PowerManagementPolicy policy;
  policy.set_idle_action(PowerManagementPolicy_Action_STOP_SESSION);
  controller_.HandlePolicyChange(policy);
  controller_.HandleUpdaterStateChange(UPDATER_UPDATING);
  TriggerDefaultAcTimeouts();
  EXPECT_EQ(JoinActions(kScreenDim, kScreenOff, kStopSession, NULL),
            delegate_.GetActions());
}

}  // namespace policy
}  // namespace power_manager
