// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/policy/suspender.h"

#include <base/basictypes.h>
#include <base/bind.h>
#include <base/callback.h>
#include <base/compiler_specific.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>

#include "power_manager/common/action_recorder.h"
#include "power_manager/common/dbus_sender_stub.h"
#include "power_manager/common/fake_prefs.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/powerd/policy/dark_resume_policy.h"

namespace power_manager {
namespace policy {

namespace {

// Various actions that can be returned by TestDelegate::GetActions().
const char kAnnounce[] = "announce";
const char kPrepare[] = "prepare";
const char kCancel[] = "cancel";
const char kSuspend[] = "suspend";
const char kAttemptComplete[] = "attempt_complete";
const char kRequestCanceled[] = "request_canceled";
const char kShutDown[] = "shut_down";
const char kNoActions[] = "";

// Test implementation of Suspender::Delegate that just records the actions it
// was asked to perform.
class TestDelegate : public Suspender::Delegate, public ActionRecorder {
 public:
  TestDelegate()
      : lid_closed_(false),
        report_success_for_get_wakeup_count_(true),
        suspend_result_(SUSPEND_SUCCESSFUL),
        wakeup_count_(0),
        suspend_wakeup_count_(0),
        suspend_wakeup_count_valid_(false),
        suspend_was_successful_(false),
        num_suspend_attempts_(0) {
  }

  void set_lid_closed(bool closed) { lid_closed_ = closed; }
  void set_report_success_for_get_wakeup_count(bool success) {
    report_success_for_get_wakeup_count_ = success;
  }
  void set_suspend_result(SuspendResult result) {
    suspend_result_ = result;
  }
  void set_wakeup_count(uint64 count) { wakeup_count_ = count; }
  void set_suspend_callback(base::Closure callback) {
    suspend_callback_ = callback;
  }
  void set_completion_callback(base::Closure callback) {
    completion_callback_ = callback;
  }

  uint64 suspend_wakeup_count() const { return suspend_wakeup_count_; }
  bool suspend_wakeup_count_valid() const {
    return suspend_wakeup_count_valid_;
  }
  bool suspend_was_successful() const { return suspend_was_successful_; }
  int num_suspend_attempts() const { return num_suspend_attempts_; }

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

  virtual SuspendResult Suspend(uint64 wakeup_count,
                                bool wakeup_count_valid,
                                base::TimeDelta duration) OVERRIDE {
    AppendAction(kSuspend);
    suspend_wakeup_count_ = wakeup_count;
    suspend_wakeup_count_valid_ = wakeup_count_valid;
    suspend_duration_ = duration;
    if (!suspend_callback_.is_null()) {
      base::Closure cb = suspend_callback_;
      suspend_callback_.Reset();
      cb.Run();
    }
    return suspend_result_;
  }

  virtual void PrepareForSuspend() { AppendAction(kPrepare); }

  virtual void HandleSuspendAttemptCompletion(
      bool success,
      int num_suspend_attempts) OVERRIDE {
    AppendAction(kAttemptComplete);
    suspend_was_successful_ = success;
    num_suspend_attempts_ = num_suspend_attempts;
    if (!completion_callback_.is_null()) {
      base::Closure cb = completion_callback_;
      completion_callback_.Reset();
      cb.Run();
    }
  }

  virtual void HandleCanceledSuspendRequest(int num_suspend_attempts) OVERRIDE {
    AppendAction(kRequestCanceled);
    num_suspend_attempts_ = num_suspend_attempts;
  }

  virtual void ShutDownForFailedSuspend() OVERRIDE {
    AppendAction(kShutDown);
  }

  virtual void ShutDownForDarkResume() OVERRIDE { AppendAction(kShutDown); }

 private:
  // Value returned by IsLidClosed().
  bool lid_closed_;

  // Should GetWakeupCount() and Suspend() report success?
  bool report_success_for_get_wakeup_count_;
  SuspendResult suspend_result_;

  // Count that should be returned by GetWakeupCount().
  uint64 wakeup_count_;

  // Callback that will be run once (if non-null) when Suspend() is called.
  base::Closure suspend_callback_;

  // Callback that will be run once (if non-null) when
  // HandleSuspendAttemptCompletion() is called.
  base::Closure completion_callback_;

  // Arguments passed to latest invocation of Suspend().
  uint64 suspend_wakeup_count_;
  bool suspend_wakeup_count_valid_;
  base::TimeDelta suspend_duration_;

  // Arguments passed to last invocation of HandleSuspendAttemptCompletion().
  // |num_suspend_attempts_| is also updated in response to
  // HandleCanceledSuspendRequest().
  bool suspend_was_successful_;
  int num_suspend_attempts_;

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

}  // namespace

class SuspenderTest : public testing::Test {
 public:
  SuspenderTest()
      : test_api_(&suspender_),
        pref_retry_delay_ms_(10000),
        pref_num_retries_(10) {
  }

 protected:
  void Init() {
    prefs_.SetInt64(kRetrySuspendMsPref, pref_retry_delay_ms_);
    prefs_.SetInt64(kRetrySuspendAttemptsPref, pref_num_retries_);
    dark_resume_policy_.Init(NULL, &prefs_);
    suspender_.Init(&delegate_, &dbus_sender_, &dark_resume_policy_, &prefs_);
  }

  FakePrefs prefs_;
  TestDelegate delegate_;
  DBusSenderStub dbus_sender_;
  DarkResumePolicy dark_resume_policy_;
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
  const int suspend_id = test_api_.suspend_id();
  SuspendImminent imminent_proto;
  EXPECT_TRUE(
      dbus_sender_.GetSentSignal(0, kSuspendImminentSignal, &imminent_proto));
  EXPECT_EQ(suspend_id, imminent_proto.suspend_id());
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
  suspender_.OnReadyForSuspend(suspend_id);
  EXPECT_EQ(JoinActions(kPrepare, kSuspend, kAttemptComplete, NULL),
            delegate_.GetActions());
  EXPECT_EQ(kWakeupCount, delegate_.suspend_wakeup_count());
  EXPECT_TRUE(delegate_.suspend_wakeup_count_valid());
  EXPECT_TRUE(delegate_.suspend_was_successful());
  EXPECT_EQ(1, delegate_.num_suspend_attempts());

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

  // A SuspendDone signal should be emitted to announce that the attempt is
  // complete.
  SuspendDone done_proto;
  EXPECT_TRUE(dbus_sender_.GetSentSignal(2, kSuspendDoneSignal, &done_proto));
  EXPECT_EQ(suspend_id, done_proto.suspend_id());
  EXPECT_EQ((kResumeTime - kSuspendTime).ToInternalValue(),
            done_proto.suspend_duration());

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
  EXPECT_EQ(JoinActions(kPrepare, kSuspend, kAttemptComplete, NULL),
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
  delegate_.set_suspend_result(Suspender::Delegate::SUSPEND_FAILED);
  suspender_.RequestSuspend();
  EXPECT_EQ(kAnnounce, delegate_.GetActions());
  int orig_suspend_id = test_api_.suspend_id();

  dbus_sender_.ClearSentSignals();
  suspender_.OnReadyForSuspend(orig_suspend_id);
  EXPECT_EQ(JoinActions(kPrepare, kSuspend, kAttemptComplete, NULL),
            delegate_.GetActions());
  EXPECT_EQ(kOrigWakeupCount, delegate_.suspend_wakeup_count());
  EXPECT_TRUE(delegate_.suspend_wakeup_count_valid());
  EXPECT_FALSE(delegate_.suspend_was_successful());
  EXPECT_EQ(1, delegate_.num_suspend_attempts());
  // TODO(derat): Stop skipping the SuspendStateChanged signal after it's been
  // removed: http://crbug.com/359619
  SuspendDone done_proto;
  EXPECT_TRUE(dbus_sender_.GetSentSignal(1, kSuspendDoneSignal, &done_proto));
  EXPECT_EQ(orig_suspend_id, done_proto.suspend_id());

  // The timeout should trigger a new suspend announcement.
  const uint64 kRetryWakeupCount = 67;
  delegate_.set_wakeup_count(kRetryWakeupCount);
  EXPECT_TRUE(test_api_.TriggerRetryTimeout());
  EXPECT_EQ(kAnnounce, delegate_.GetActions());
  int new_suspend_id = test_api_.suspend_id();
  EXPECT_NE(orig_suspend_id, new_suspend_id);

  dbus_sender_.ClearSentSignals();
  suspender_.OnReadyForSuspend(new_suspend_id);
  EXPECT_EQ(JoinActions(kPrepare, kSuspend, kAttemptComplete, NULL),
            delegate_.GetActions());
  EXPECT_EQ(kRetryWakeupCount, delegate_.suspend_wakeup_count());
  EXPECT_TRUE(delegate_.suspend_wakeup_count_valid());
  EXPECT_FALSE(delegate_.suspend_was_successful());
  EXPECT_EQ(2, delegate_.num_suspend_attempts());
  EXPECT_TRUE(dbus_sender_.GetSentSignal(1, kSuspendDoneSignal, &done_proto));
  EXPECT_EQ(new_suspend_id, done_proto.suspend_id());

  // Explicitly requesting a suspend should cancel the timeout and generate a
  // new announcement immediately.
  suspender_.RequestSuspend();
  EXPECT_EQ(kAnnounce, delegate_.GetActions());
  int final_suspend_id = test_api_.suspend_id();
  EXPECT_NE(new_suspend_id, final_suspend_id);
  EXPECT_FALSE(test_api_.TriggerRetryTimeout());

  // Report success this time and check that the timeout isn't registered.
  dbus_sender_.ClearSentSignals();
  delegate_.set_suspend_result(Suspender::Delegate::SUSPEND_SUCCESSFUL);
  suspender_.OnReadyForSuspend(final_suspend_id);
  EXPECT_EQ(JoinActions(kPrepare, kSuspend, kAttemptComplete, NULL),
            delegate_.GetActions());
  EXPECT_TRUE(delegate_.suspend_was_successful());
  EXPECT_EQ(3, delegate_.num_suspend_attempts());
  EXPECT_FALSE(test_api_.TriggerRetryTimeout());
  EXPECT_TRUE(dbus_sender_.GetSentSignal(2, kSuspendDoneSignal, &done_proto));
  EXPECT_EQ(final_suspend_id, done_proto.suspend_id());

  // Suspend successfully again and check that the number of attempts are
  // reported as 1 now.
  suspender_.RequestSuspend();
  EXPECT_EQ(kAnnounce, delegate_.GetActions());
  suspender_.OnReadyForSuspend(test_api_.suspend_id());
  EXPECT_EQ(JoinActions(kPrepare, kSuspend, kAttemptComplete, NULL),
            delegate_.GetActions());
  EXPECT_TRUE(delegate_.suspend_was_successful());
  EXPECT_EQ(1, delegate_.num_suspend_attempts());
  EXPECT_FALSE(test_api_.TriggerRetryTimeout());
}

// Tests that the system is shut down after repeated suspend failures.
TEST_F(SuspenderTest, ShutDownAfterRepeatedFailures) {
  pref_num_retries_ = 5;
  Init();

  delegate_.set_suspend_result(Suspender::Delegate::SUSPEND_FAILED);
  suspender_.RequestSuspend();
  EXPECT_EQ(kAnnounce, delegate_.GetActions());
  suspender_.OnReadyForSuspend(test_api_.suspend_id());
  EXPECT_EQ(JoinActions(kPrepare, kSuspend, kAttemptComplete, NULL),
            delegate_.GetActions());
  EXPECT_FALSE(delegate_.suspend_was_successful());
  EXPECT_EQ(1, delegate_.num_suspend_attempts());

  // Proceed through all retries, reporting failure each time.
  for (int i = 1; i <= pref_num_retries_; ++i) {
    EXPECT_TRUE(test_api_.TriggerRetryTimeout()) << "Retry #" << i;
    EXPECT_EQ(kAnnounce, delegate_.GetActions()) << "Retry #" << i;
    suspender_.OnReadyForSuspend(test_api_.suspend_id());
    EXPECT_EQ(JoinActions(kPrepare, kSuspend, kAttemptComplete, NULL),
              delegate_.GetActions())
        << "Retry #" << i;
    EXPECT_FALSE(delegate_.suspend_was_successful());
    EXPECT_EQ(i + 1, delegate_.num_suspend_attempts());
  }

  // On the last failed attempt, the system should shut down immediately.
  EXPECT_TRUE(test_api_.TriggerRetryTimeout());
  EXPECT_EQ(kShutDown, delegate_.GetActions());
  EXPECT_FALSE(test_api_.TriggerRetryTimeout());
}

// Tests that OnReadyForSuspend() doesn't trigger a call to Suspend() if
// activity that should cancel the current suspend attempt was previously
// received.
TEST_F(SuspenderTest, CancelBeforeSuspend) {
  Init();

  // User activity should cancel suspending.  Suspender should manually
  // emit a PowerStateChanged signal since powerd_suspend didn't run.
  dbus_sender_.ClearSentSignals();
  suspender_.RequestSuspend();
  EXPECT_EQ(kAnnounce, delegate_.GetActions());
  suspender_.HandleUserActivity();

  // Skip the SuspendImminent signal at position 0.
  SuspendDone done_proto;
  EXPECT_TRUE(dbus_sender_.GetSentSignal(1, kSuspendDoneSignal, &done_proto));
  EXPECT_EQ(test_api_.suspend_id(), done_proto.suspend_id());
  EXPECT_EQ(base::TimeDelta().ToInternalValue(), done_proto.suspend_duration());
  EXPECT_EQ(JoinActions(kCancel, kRequestCanceled, NULL),
            delegate_.GetActions());
  EXPECT_EQ(1, delegate_.num_suspend_attempts());
  suspender_.OnReadyForSuspend(test_api_.suspend_id());
  EXPECT_EQ(kNoActions, delegate_.GetActions());
  EXPECT_FALSE(test_api_.TriggerRetryTimeout());

  // The lid being opened should also cancel.
  dbus_sender_.ClearSentSignals();
  suspender_.RequestSuspend();
  EXPECT_EQ(kAnnounce, delegate_.GetActions());
  suspender_.HandleLidOpened();
  EXPECT_TRUE(dbus_sender_.GetSentSignal(1, kSuspendDoneSignal, &done_proto));
  EXPECT_EQ(JoinActions(kCancel, kRequestCanceled, NULL),
            delegate_.GetActions());
  EXPECT_EQ(1, delegate_.num_suspend_attempts());
  suspender_.OnReadyForSuspend(test_api_.suspend_id());
  EXPECT_EQ(kNoActions, delegate_.GetActions());
  EXPECT_FALSE(test_api_.TriggerRetryTimeout());

  // The request should also be canceled if the system starts shutting down.
  dbus_sender_.ClearSentSignals();
  suspender_.RequestSuspend();
  EXPECT_EQ(kAnnounce, delegate_.GetActions());
  suspender_.HandleShutdown();
  EXPECT_TRUE(dbus_sender_.GetSentSignal(1, kSuspendDoneSignal, &done_proto));
  EXPECT_EQ(JoinActions(kCancel, kRequestCanceled, NULL),
            delegate_.GetActions());
  EXPECT_EQ(1, delegate_.num_suspend_attempts());
  suspender_.OnReadyForSuspend(test_api_.suspend_id());
  EXPECT_EQ(kNoActions, delegate_.GetActions());
  EXPECT_FALSE(test_api_.TriggerRetryTimeout());

  // Subsequent requests after shutdown has started should be ignored.
  suspender_.RequestSuspend();
  EXPECT_EQ(kNoActions, delegate_.GetActions());
}

// Tests that a suspend-canceling action after a failed suspend attempt
// should remove the retry timeout.
TEST_F(SuspenderTest, CancelAfterSuspend) {
  Init();
  delegate_.set_suspend_result(Suspender::Delegate::SUSPEND_FAILED);
  suspender_.RequestSuspend();
  EXPECT_EQ(kAnnounce, delegate_.GetActions());
  suspender_.OnReadyForSuspend(test_api_.suspend_id());
  EXPECT_EQ(JoinActions(kPrepare, kSuspend, kAttemptComplete, NULL),
            delegate_.GetActions());
  EXPECT_FALSE(delegate_.suspend_was_successful());
  EXPECT_EQ(1, delegate_.num_suspend_attempts());

  // Fail a second time.
  EXPECT_TRUE(test_api_.TriggerRetryTimeout());
  EXPECT_EQ(kAnnounce, delegate_.GetActions());
  suspender_.OnReadyForSuspend(test_api_.suspend_id());
  EXPECT_EQ(JoinActions(kPrepare, kSuspend, kAttemptComplete, NULL),
            delegate_.GetActions());
  EXPECT_FALSE(delegate_.suspend_was_successful());
  EXPECT_EQ(2, delegate_.num_suspend_attempts());

  // This time, also report user activity, which should cancel the request.
  suspender_.HandleUserActivity();
  EXPECT_FALSE(test_api_.TriggerRetryTimeout());
  EXPECT_EQ(kRequestCanceled, delegate_.GetActions());
  EXPECT_EQ(2, delegate_.num_suspend_attempts());
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
  EXPECT_EQ(JoinActions(kPrepare, kSuspend, kAttemptComplete, NULL),
            delegate_.GetActions());

  // Report user activity after powerd_suspend fails and check that the
  // retry timeout isn't removed.
  delegate_.set_suspend_result(Suspender::Delegate::SUSPEND_CANCELED);
  suspender_.RequestSuspend();
  EXPECT_EQ(kAnnounce, delegate_.GetActions());
  suspender_.OnReadyForSuspend(test_api_.suspend_id());
  EXPECT_EQ(JoinActions(kPrepare, kSuspend, kAttemptComplete, NULL),
            delegate_.GetActions());
  suspender_.HandleUserActivity();

  delegate_.set_suspend_result(Suspender::Delegate::SUSPEND_SUCCESSFUL);
  EXPECT_TRUE(test_api_.TriggerRetryTimeout());
  EXPECT_EQ(kAnnounce, delegate_.GetActions());
  suspender_.OnReadyForSuspend(test_api_.suspend_id());
  EXPECT_EQ(JoinActions(kPrepare, kSuspend, kAttemptComplete, NULL),
            delegate_.GetActions());
}

// Tests that expected wakeup counts passed to
// RequestSuspendWithExternalWakeupCount() are honored.
TEST_F(SuspenderTest, ExternalWakeupCount) {
  Init();

  // Pass a wakeup count less than the one that the delegate returns.
  const uint64 kWakeupCount = 452;
  delegate_.set_wakeup_count(kWakeupCount);
  suspender_.RequestSuspendWithExternalWakeupCount(kWakeupCount - 1);
  EXPECT_EQ(kAnnounce, delegate_.GetActions());

  // Make the delegate report that powerd_suspend reported a wakeup count
  // mismatch. Suspender should avoid retrying after a mismatch when using an
  // external wakeup count.
  delegate_.set_suspend_result(Suspender::Delegate::SUSPEND_CANCELED);
  suspender_.OnReadyForSuspend(test_api_.suspend_id());
  EXPECT_EQ(JoinActions(kPrepare, kSuspend, kAttemptComplete, NULL),
            delegate_.GetActions());
  EXPECT_EQ(kWakeupCount - 1, delegate_.suspend_wakeup_count());
  EXPECT_FALSE(test_api_.TriggerRetryTimeout());

  // Send another suspend request with the current wakeup count. Report failure
  // and check that the suspend attempt is retried.
  suspender_.RequestSuspendWithExternalWakeupCount(kWakeupCount);
  EXPECT_EQ(kAnnounce, delegate_.GetActions());
  delegate_.set_suspend_result(Suspender::Delegate::SUSPEND_FAILED);
  suspender_.OnReadyForSuspend(test_api_.suspend_id());
  EXPECT_EQ(JoinActions(kPrepare, kSuspend, kAttemptComplete, NULL),
            delegate_.GetActions());
  EXPECT_EQ(kWakeupCount, delegate_.suspend_wakeup_count());

  // Let the retry succeed and check that another retry isn't scheduled.
  EXPECT_TRUE(test_api_.TriggerRetryTimeout());
  EXPECT_EQ(kAnnounce, delegate_.GetActions());
  delegate_.set_suspend_result(Suspender::Delegate::SUSPEND_SUCCESSFUL);
  suspender_.OnReadyForSuspend(test_api_.suspend_id());
  EXPECT_EQ(JoinActions(kPrepare, kSuspend, kAttemptComplete, NULL),
            delegate_.GetActions());
  EXPECT_EQ(kWakeupCount, delegate_.suspend_wakeup_count());
  EXPECT_FALSE(test_api_.TriggerRetryTimeout());
}

// Tests that the SuspendDone signal contains a zero duration rather than a
// negative duration if the system clock jumps backward between suspend and
// resume.
TEST_F(SuspenderTest, SystemClockGoesBackward) {
  Init();
  suspender_.RequestSuspend();

  test_api_.SetCurrentWallTime(base::Time::FromInternalValue(5000));
  delegate_.set_suspend_callback(
      base::Bind(&Suspender::TestApi::SetCurrentWallTime,
                 base::Unretained(&test_api_),
                 base::Time::FromInternalValue(1000)));
  dbus_sender_.ClearSentSignals();
  suspender_.OnReadyForSuspend(test_api_.suspend_id());
  SuspendDone done_proto;
  EXPECT_TRUE(dbus_sender_.GetSentSignal(2, kSuspendDoneSignal, &done_proto));
  EXPECT_EQ(base::TimeDelta().ToInternalValue(), done_proto.suspend_duration());
}

// Tests that things don't go haywire when
// Suspender::Delegate::HandleSuspendAttemptCompletion() synchronously requests
// another suspend attempt. Previously, this could result in the new suspend
// attempt being started before the previous one had completed.
TEST_F(SuspenderTest, CompletionCallbackStartsNewAttempt) {
  // Instruct the delegate to report failure and to start another suspend
  // attempt when the current one finishes.
  Init();
  suspender_.RequestSuspend();
  delegate_.set_suspend_result(Suspender::Delegate::SUSPEND_FAILED);
  delegate_.set_completion_callback(
      base::Bind(&Suspender::RequestSuspend, base::Unretained(&suspender_)));

  // Check that the SuspendDone signal from the first attempt contains the first
  // attempt's ID.
  dbus_sender_.ClearSentSignals();
  const int kOldSuspendId = test_api_.suspend_id();
  suspender_.OnReadyForSuspend(kOldSuspendId);
  SuspendDone done_proto;
  EXPECT_TRUE(dbus_sender_.GetSentSignal(1, kSuspendDoneSignal, &done_proto));
  EXPECT_EQ(kOldSuspendId, done_proto.suspend_id());
}

}  // namespace policy
}  // namespace power_manager
