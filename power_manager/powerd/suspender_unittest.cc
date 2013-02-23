// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/common/dbus_sender_stub.h"
#include "power_manager/common/fake_prefs.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/powerd/policy/dark_resume_policy.h"
#include "power_manager/powerd/suspender.h"

namespace power_manager {

namespace {

// Various actions that can be returned by TestDelegate::GetActions().
const char kAnnounce[] = "announce";
const char kPrepare[] = "prepare";
const char kCancel[] = "cancel";
const char kSuspend[] = "suspend";
const char kResume[] = "resume";
const char kShutdown[] = "shutdown";
const char kNoActions[] = "";

// Joins a sequence of strings describing actions (e.g. kScreenDim) such
// that they can be compared against a string returned by
// TestDelegate::GetActions().  The list of actions must be terminated by a
// NULL pointer.
// TODO(derat): Move this and state_controller_unittest.cc's implementation
// into a shared library.
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

// Test implementation of Suspender::Delegate that just records the actions it
// was asked to perform.
class TestDelegate : public Suspender::Delegate {
 public:
  TestDelegate()
      : lid_closed_(false),
        report_success_for_get_wakeup_count_(true),
        report_success_for_suspend_(true),
        wakeup_count_(0),
        suspend_wakeup_count_(0),
        suspend_wakeup_count_valid_(false),
        suspend_duration_() {
  }

  void set_lid_closed(bool closed) { lid_closed_ = closed; }
  void set_report_success_for_get_wakeup_count(bool success) {
    report_success_for_get_wakeup_count_ = success;
  }
  void set_report_success_for_suspend(bool success) {
    report_success_for_suspend_ = success;
  }
  void set_wakeup_count(uint64 count) { wakeup_count_ = count; }
  void set_suspend_callback(base::Closure callback) {
    suspend_callback_ = callback;
  }

  uint64 suspend_wakeup_count() const { return suspend_wakeup_count_; }
  bool suspend_wakeup_count_valid() const {
    return suspend_wakeup_count_valid_;
  }

  // Returns a comma-separated, oldest-first list of the calls made to the
  // delegate since the last call to GetActions().
  std::string GetActions() {
    std::string actions = actions_;
    actions_.clear();
    return actions;
  }

  // Delegate implementation:
  virtual bool IsLidClosed() OVERRIDE { return lid_closed_; }

  virtual bool GetWakeupCount(uint64* wakeup_count) OVERRIDE {
    if (!report_success_for_get_wakeup_count_)
      return false;
    *wakeup_count = wakeup_count_;
    return true;
  }

  virtual void PrepareForSuspendAnnouncement() OVERRIDE {
    AppendAction(kAnnounce);
  }

  virtual void HandleCanceledSuspendAnnouncement() OVERRIDE {
    AppendAction(kCancel);
  }

  virtual bool Suspend(uint64 wakeup_count,
                       bool wakeup_count_valid,
                       base::TimeDelta duration) OVERRIDE {
    AppendAction(kSuspend);
    suspend_wakeup_count_ = wakeup_count;
    suspend_wakeup_count_valid_ = wakeup_count_valid;
    suspend_duration_ = duration;
    if (!suspend_callback_.is_null()) {
      suspend_callback_.Run();
      suspend_callback_.Reset();
    }
    return report_success_for_suspend_;
  }

  virtual void PrepareForSuspend() { AppendAction(kPrepare); }

  virtual void HandleResume(bool success,
                            int num_retries,
                            int max_retries) OVERRIDE { AppendAction(kResume); }

  virtual void ShutdownForFailedSuspend() OVERRIDE { AppendAction(kShutdown); }

  virtual void ShutdownForDarkResume() OVERRIDE { AppendAction(kShutdown); }

 private:
  // Appends |action| to |actions_|.
  void AppendAction(const std::string& action) {
    if (!actions_.empty())
      actions_ += ",";
    actions_ += action;
  }

  // Value returned by IsLidClosed().
  bool lid_closed_;

  // Should GetWakeupCount() and Suspend() report success?
  bool report_success_for_get_wakeup_count_;
  bool report_success_for_suspend_;;

  // Count that should be returned by GetWakeupCount().
  uint64 wakeup_count_;

  // Callback that will be run (if non-null) when Suspend() is called.
  base::Closure suspend_callback_;

  // Comma-separated list describing called methods.
  std::string actions_;

  // Arguments passed to latest invocation of Suspend().
  uint64 suspend_wakeup_count_;
  bool suspend_wakeup_count_valid_;
  base::TimeDelta suspend_duration_;

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

}  // namespace

class SuspenderTest : public testing::Test {
 public:
  SuspenderTest()
      : dark_resume_policy_(NULL, &prefs_),
        suspender_(&delegate_, &dbus_sender_, &dark_resume_policy_),
        test_api_(&suspender_),
        pref_retry_delay_ms_(10000),
        pref_num_retries_(10) {
  }

 protected:
  void Init() {
    CHECK(prefs_.SetInt64(kRetrySuspendMsPref, pref_retry_delay_ms_));
    CHECK(prefs_.SetInt64(kRetrySuspendAttemptsPref, pref_num_retries_));
    suspender_.Init(&prefs_);
    dark_resume_policy_.Init();
  }

  FakePrefs prefs_;
  TestDelegate delegate_;
  DBusSenderStub dbus_sender_;
  policy::DarkResumePolicy dark_resume_policy_;
  Suspender suspender_;
  Suspender::TestApi test_api_;

  int64 pref_retry_delay_ms_;
  int64 pref_num_retries_;
};

// Tests the standard suspend/resume cycle.
TEST_F(SuspenderTest, SuspendResume) {
  Init();

  // Suspender shouldn't run powerd_suspend until it receives notice that
  // SuspendDelayController is ready.
  const uint64 kWakeupCount = 452;
  const base::Time kRequestTime = base::Time::FromInternalValue(123);
  test_api_.SetCurrentWallTime(kRequestTime);
  delegate_.set_wakeup_count(kWakeupCount);
  suspender_.RequestSuspend();
  EXPECT_EQ(kAnnounce, delegate_.GetActions());

  // Advance the time and register a callback to advance the time again
  // when the suspend request is received.
  const base::Time kSuspendTime = base::Time::FromInternalValue(301);
  test_api_.SetCurrentWallTime(kSuspendTime);
  const base::Time kResumeTime = base::Time::FromInternalValue(567);
  delegate_.set_suspend_callback(
      base::Bind(&Suspender::TestApi::SetCurrentWallTime,
                 base::Unretained(&test_api_), kResumeTime));

  // When Suspender receives notice that the system is ready to be
  // suspended, it should immediately suspend the system.
  dbus_sender_.ClearSentSignals();
  suspender_.OnReadyForSuspend(test_api_.suspend_id());
  EXPECT_EQ(JoinActions(kPrepare, kSuspend, kResume, NULL),
            delegate_.GetActions());
  EXPECT_EQ(kWakeupCount, delegate_.suspend_wakeup_count());
  EXPECT_TRUE(delegate_.suspend_wakeup_count_valid());

  // Just before Suspender suspends the system, it should emit a
  // SuspendStateChanged signal with the current wall time.
  SuspendState proto;
  EXPECT_TRUE(
      dbus_sender_.GetSentSignal(0, kSuspendStateChangedSignal, &proto));
  EXPECT_EQ(SuspendState_Type_SUSPEND_TO_MEMORY, proto.type());
  EXPECT_EQ(kSuspendTime.ToInternalValue(), proto.wall_time());

  // After suspend/resume completes, it should emit a second signal with an
  // updated timestamp.
  EXPECT_TRUE(
      dbus_sender_.GetSentSignal(1, kSuspendStateChangedSignal, &proto));
  EXPECT_EQ(SuspendState_Type_RESUME, proto.type());
  EXPECT_EQ(kResumeTime.ToInternalValue(), proto.wall_time());

  // A retry timeout shouldn't be set.
  EXPECT_FALSE(test_api_.TriggerRetryTimeout());
}

// Tests that Suspender doesn't pass a wakeup count to the delegate when it was
// unable to fetch one.
TEST_F(SuspenderTest, MissingWakeupCount) {
  Init();

  delegate_.set_report_success_for_get_wakeup_count(false);
  suspender_.RequestSuspend();
  EXPECT_EQ(kAnnounce, delegate_.GetActions());
  suspender_.OnReadyForSuspend(test_api_.suspend_id());
  EXPECT_EQ(JoinActions(kPrepare, kSuspend, kResume, NULL),
            delegate_.GetActions());
  EXPECT_FALSE(delegate_.suspend_wakeup_count_valid());
}

// Tests that calls to RequestSuspend() are ignored when a suspend request is
// already underway.
TEST_F(SuspenderTest, IgnoreDuplicateSuspendRequests) {
  Init();

  suspender_.RequestSuspend();
  EXPECT_EQ(kAnnounce, delegate_.GetActions());
  int orig_suspend_id = test_api_.suspend_id();

  // The suspend ID should be left unchanged after a second call to
  // RequestSuspend().
  suspender_.RequestSuspend();
  EXPECT_EQ(kNoActions, delegate_.GetActions());
  EXPECT_EQ(orig_suspend_id, test_api_.suspend_id());
}

// Tests that suspend is retried on failure.
TEST_F(SuspenderTest, RetryOnFailure) {
  Init();

  const uint64 kOrigWakeupCount = 46;
  delegate_.set_wakeup_count(kOrigWakeupCount);
  delegate_.set_report_success_for_suspend(false);
  suspender_.RequestSuspend();
  EXPECT_EQ(kAnnounce, delegate_.GetActions());
  int orig_suspend_id = test_api_.suspend_id();

  suspender_.OnReadyForSuspend(orig_suspend_id);
  EXPECT_EQ(JoinActions(kPrepare, kSuspend, kResume, NULL),
            delegate_.GetActions());
  EXPECT_EQ(kOrigWakeupCount, delegate_.suspend_wakeup_count());
  EXPECT_TRUE(delegate_.suspend_wakeup_count_valid());

  // The timeout should trigger a new suspend announcement.
  const uint64 kRetryWakeupCount = 67;
  delegate_.set_wakeup_count(kRetryWakeupCount);
  EXPECT_TRUE(test_api_.TriggerRetryTimeout());
  EXPECT_EQ(kAnnounce, delegate_.GetActions());
  int new_suspend_id = test_api_.suspend_id();
  EXPECT_NE(orig_suspend_id, new_suspend_id);

  suspender_.OnReadyForSuspend(new_suspend_id);
  EXPECT_EQ(JoinActions(kPrepare, kSuspend, kResume, NULL),
            delegate_.GetActions());
  EXPECT_EQ(kRetryWakeupCount, delegate_.suspend_wakeup_count());
  EXPECT_TRUE(delegate_.suspend_wakeup_count_valid());

  // Explicitly requesting a suspend should cancel the timeout and generate a
  // new announcement immediately.
  suspender_.RequestSuspend();
  EXPECT_EQ(kAnnounce, delegate_.GetActions());
  int final_suspend_id = test_api_.suspend_id();
  EXPECT_NE(new_suspend_id, final_suspend_id);
  EXPECT_FALSE(test_api_.TriggerRetryTimeout());

  // Report success this time and check that the timeout isn't registered.
  delegate_.set_report_success_for_suspend(true);
  suspender_.OnReadyForSuspend(final_suspend_id);
  EXPECT_EQ(JoinActions(kPrepare, kSuspend, kResume, NULL),
            delegate_.GetActions());
  EXPECT_FALSE(test_api_.TriggerRetryTimeout());
}

// Tests that the system is shut down after repeated suspend failures.
TEST_F(SuspenderTest, ShutdownAfterRepeatedFailures) {
  pref_num_retries_ = 5;
  Init();

  delegate_.set_report_success_for_suspend(false);
  suspender_.RequestSuspend();
  EXPECT_EQ(kAnnounce, delegate_.GetActions());
  suspender_.OnReadyForSuspend(test_api_.suspend_id());
  EXPECT_EQ(JoinActions(kPrepare, kSuspend, kResume, NULL),
            delegate_.GetActions());

  // Proceed through all retries, reporting failure each time.
  for (int i = 1; i <= pref_num_retries_; ++i) {
    EXPECT_TRUE(test_api_.TriggerRetryTimeout()) << "Retry #" << i;
    EXPECT_EQ(kAnnounce, delegate_.GetActions()) << "Retry #" << i;
    suspender_.OnReadyForSuspend(test_api_.suspend_id());
    EXPECT_EQ(JoinActions(kPrepare, kSuspend, kResume, NULL),
              delegate_.GetActions())
        << "Retry #" << i;
  }

  // On the last failed attempt, the system should shut down immediately.
  EXPECT_TRUE(test_api_.TriggerRetryTimeout());
  EXPECT_EQ(kShutdown, delegate_.GetActions());
  EXPECT_FALSE(test_api_.TriggerRetryTimeout());
}

// Tests that OnReadyForSuspend() doesn't trigger a call to Suspend() if
// activity that should cancel the current suspend attempt was previously
// received.
TEST_F(SuspenderTest, CancelBeforeSuspend) {
  Init();

  // User activity should cancel suspending.  Suspender should manually
  // emit a PowerStateChanged signal since powerd_suspend didn't run.
  suspender_.RequestSuspend();
  EXPECT_EQ(kAnnounce, delegate_.GetActions());
  suspender_.HandleUserActivity();
  EXPECT_EQ(kCancel, delegate_.GetActions());
  suspender_.OnReadyForSuspend(test_api_.suspend_id());
  EXPECT_EQ(kNoActions, delegate_.GetActions());
  EXPECT_FALSE(test_api_.TriggerRetryTimeout());

  // The lid being opened should also cancel.
  suspender_.RequestSuspend();
  EXPECT_EQ(kAnnounce, delegate_.GetActions());
  suspender_.HandleLidOpened();
  EXPECT_EQ(kCancel, delegate_.GetActions());
  suspender_.OnReadyForSuspend(test_api_.suspend_id());
  EXPECT_EQ(kNoActions, delegate_.GetActions());
  EXPECT_FALSE(test_api_.TriggerRetryTimeout());

  // The request should also be canceled if the system starts shutting down.
  suspender_.RequestSuspend();
  EXPECT_EQ(kAnnounce, delegate_.GetActions());
  suspender_.HandleShutdown();
  EXPECT_EQ(kCancel, delegate_.GetActions());
  suspender_.OnReadyForSuspend(test_api_.suspend_id());
  EXPECT_EQ(kNoActions, delegate_.GetActions());
  EXPECT_FALSE(test_api_.TriggerRetryTimeout());
}

// Tests that the a suspend-canceling action after a failed suspend attempt
// should remove the retry timeout.
TEST_F(SuspenderTest, CancelAfterSuspend) {
  Init();
  delegate_.set_report_success_for_suspend(false);
  suspender_.RequestSuspend();
  EXPECT_EQ(kAnnounce, delegate_.GetActions());
  suspender_.OnReadyForSuspend(test_api_.suspend_id());
  EXPECT_EQ(JoinActions(kPrepare, kSuspend, kResume, NULL),
            delegate_.GetActions());
  suspender_.HandleUserActivity();
  EXPECT_FALSE(test_api_.TriggerRetryTimeout());
}

// Tests that Chrome-reported user activity received while suspending with
// a closed lid doesn't abort the suspend attempt (http://crosbug.com/38819).
TEST_F(SuspenderTest, DontCancelForUserActivityWhileLidClosed) {
  delegate_.set_lid_closed(true);
  Init();

  // Report user activity before powerd_suspend is executed and check that
  // Suspender still suspends when OnReadyForSuspend() is called.
  suspender_.RequestSuspend();
  EXPECT_EQ(kAnnounce, delegate_.GetActions());
  suspender_.HandleUserActivity();
  suspender_.OnReadyForSuspend(test_api_.suspend_id());
  EXPECT_EQ(JoinActions(kPrepare, kSuspend, kResume, NULL),
            delegate_.GetActions());

  // Report user activity after powerd_suspend fails and check that the
  // retry timeout isn't removed.
  delegate_.set_report_success_for_suspend(false);
  suspender_.RequestSuspend();
  EXPECT_EQ(kAnnounce, delegate_.GetActions());
  suspender_.OnReadyForSuspend(test_api_.suspend_id());
  EXPECT_EQ(JoinActions(kPrepare, kSuspend, kResume, NULL),
            delegate_.GetActions());
  suspender_.HandleUserActivity();

  delegate_.set_report_success_for_suspend(true);
  EXPECT_TRUE(test_api_.TriggerRetryTimeout());
  EXPECT_EQ(kAnnounce, delegate_.GetActions());
  suspender_.OnReadyForSuspend(test_api_.suspend_id());
  EXPECT_EQ(JoinActions(kPrepare, kSuspend, kResume, NULL),
            delegate_.GetActions());
}

}  // namespace power_manager
