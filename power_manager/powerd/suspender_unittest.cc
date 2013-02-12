// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/common/dbus_sender_stub.h"
#include "power_manager/common/fake_prefs.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/powerd/suspender.h"

namespace power_manager {

namespace {

// Various actions that can be returned by TestDelegate::GetActions().
const char kSuspend[] = "suspend";
const char kCancel[] = "cancel";
const char kEmit[] = "emit";
const char kResume[] = "resume";
const char kShutdown[] = "shutdown";
const char kNoActions[] = "";

// Test implementation of Suspender::Delegate that just records the actions it
// was asked to perform.
class TestDelegate : public Suspender::Delegate {
 public:
  TestDelegate()
      : lid_closed_(false),
        report_success_for_get_wakeup_count_(true),
        wakeup_count_(0),
        suspend_wakeup_count_(0),
        suspend_wakeup_count_valid_(false),
        suspend_suspend_id_(-1),
        emit_suspend_id_(-1) {
  }

  void set_lid_closed(bool closed) { lid_closed_ = closed; }
  void set_report_success_for_get_wakeup_count(bool success) {
    report_success_for_get_wakeup_count_ = success;
  }
  void set_wakeup_count(uint64 count) { wakeup_count_ = count; }
  uint64 suspend_wakeup_count() const { return suspend_wakeup_count_; }
  bool suspend_wakeup_count_valid() const {
    return suspend_wakeup_count_valid_;
  }
  int suspend_suspend_id() const { return suspend_suspend_id_; }
  int emit_suspend_id() const { return emit_suspend_id_; }

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

  virtual void Suspend(uint64 wakeup_count,
                       bool wakeup_count_valid,
                       int suspend_id) OVERRIDE {
    AppendAction(kSuspend);
    suspend_wakeup_count_ = wakeup_count;
    suspend_wakeup_count_valid_ = wakeup_count_valid;
    suspend_suspend_id_ = suspend_id;
  }

  virtual void CancelSuspend() OVERRIDE { AppendAction(kCancel); }

  virtual void EmitPowerStateChangedOnSignal(int suspend_id) OVERRIDE {
    AppendAction(kEmit);
    emit_suspend_id_ = suspend_id;
  }

  virtual void HandleResume(bool success,
                            int num_retries,
                            int max_retries) OVERRIDE { AppendAction(kResume); }

  virtual void ShutdownForFailedSuspend() OVERRIDE { AppendAction(kShutdown); }

 private:
  // Appends |action| to |actions_|.
  void AppendAction(const std::string& action) {
    if (!actions_.empty())
      actions_ += ",";
    actions_ += action;
  }

  // Value returned by IsLidClosed().
  bool lid_closed_;

  // Should GetWakeupCount() report success?
  bool report_success_for_get_wakeup_count_;

  // Count that should be returned by GetWakeupCount().
  uint64 wakeup_count_;

  // Comma-separated list describing called methods.
  std::string actions_;

  // Arguments passed to latest invocation of Suspend().
  uint64 suspend_wakeup_count_;
  bool suspend_wakeup_count_valid_;
  int suspend_suspend_id_;

  // Argument passed to last invocation of EmitPowerStateChangedOnSignal().
  int emit_suspend_id_;

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

}  // namespace

class SuspenderTest : public testing::Test {
 public:
  SuspenderTest()
      : suspender_(&delegate_, &dbus_sender_),
        test_api_(&suspender_),
        pref_retry_delay_ms_(10000),
        pref_num_retries_(10) {
  }

 protected:
  void Init() {
    CHECK(prefs_.SetInt64(kRetrySuspendMsPref, pref_retry_delay_ms_));
    CHECK(prefs_.SetInt64(kRetrySuspendAttemptsPref, pref_num_retries_));
    suspender_.Init(&prefs_);
  }

  FakePrefs prefs_;
  TestDelegate delegate_;
  DBusSenderStub dbus_sender_;
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
  EXPECT_EQ(kNoActions, delegate_.GetActions());

  suspender_.OnReadyForSuspend(test_api_.suspend_id());
  EXPECT_EQ(kSuspend, delegate_.GetActions());
  EXPECT_EQ(kWakeupCount, delegate_.suspend_wakeup_count());
  EXPECT_TRUE(delegate_.suspend_wakeup_count_valid());
  EXPECT_EQ(test_api_.suspend_id(), delegate_.suspend_suspend_id());

  // When Suspender receives notice that the system is suspending, it
  // should emit a SuspendStateChanged signal with the wall time from the
  // original request.
  const base::Time kSuspendTime = base::Time::FromInternalValue(301);
  test_api_.SetCurrentWallTime(kSuspendTime);
  dbus_sender_.ClearSentSignals();
  suspender_.HandlePowerStateChanged(
      Suspender::kMemState, 0, test_api_.suspend_id());
  EXPECT_EQ(kNoActions, delegate_.GetActions());

  SuspendState proto;
  EXPECT_TRUE(
      dbus_sender_.GetSentSignal(0, kSuspendStateChangedSignal, &proto));
  EXPECT_EQ(SuspendState_Type_SUSPEND_TO_MEMORY, proto.type());
  EXPECT_EQ(kRequestTime.ToInternalValue(), proto.wall_time());

  // Notification that the system has resumed should trigger a call to the
  // delegate's HandleResume() method and another SuspendStateChanged
  // signal.
  const base::Time kResumeTime = base::Time::FromInternalValue(567);
  test_api_.SetCurrentWallTime(kResumeTime);
  dbus_sender_.ClearSentSignals();
  suspender_.HandlePowerStateChanged(
      Suspender::kOnState, 0, test_api_.suspend_id());
  EXPECT_EQ(kResume, delegate_.GetActions());
  EXPECT_TRUE(
      dbus_sender_.GetSentSignal(0, kSuspendStateChangedSignal, &proto));
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
  suspender_.OnReadyForSuspend(test_api_.suspend_id());
  EXPECT_EQ(kSuspend, delegate_.GetActions());
  EXPECT_FALSE(delegate_.suspend_wakeup_count_valid());
}

// Tests that calls to RequestSuspend() are ignored when a suspend request is
// already underway.
TEST_F(SuspenderTest, IgnoreDuplicateSuspendRequests) {
  Init();

  suspender_.RequestSuspend();
  EXPECT_EQ(kNoActions, delegate_.GetActions());
  int orig_suspend_id = test_api_.suspend_id();

  // The suspend ID should be left unchanged after a second call to
  // RequestSuspend().
  suspender_.RequestSuspend();
  EXPECT_EQ(kNoActions, delegate_.GetActions());
  EXPECT_EQ(orig_suspend_id, test_api_.suspend_id());

  suspender_.OnReadyForSuspend(test_api_.suspend_id());
  EXPECT_EQ(kSuspend, delegate_.GetActions());

  // Calling RequestSuspend() after powerd_suspend has been run also shouldn't
  // do anything.
  suspender_.RequestSuspend();
  EXPECT_EQ(kNoActions, delegate_.GetActions());
  EXPECT_EQ(orig_suspend_id, test_api_.suspend_id());

  suspender_.HandlePowerStateChanged(
      Suspender::kMemState, 0, test_api_.suspend_id());
  suspender_.HandlePowerStateChanged(
      Suspender::kOnState, 0, test_api_.suspend_id());
  EXPECT_EQ(kResume, delegate_.GetActions());

  // After the system suspends and resumes successfully, RequestSuspend() should
  // work as before.
  suspender_.RequestSuspend();
  EXPECT_EQ(kNoActions, delegate_.GetActions());
  EXPECT_GT(test_api_.suspend_id(), orig_suspend_id);

  suspender_.OnReadyForSuspend(test_api_.suspend_id());
  EXPECT_EQ(kSuspend, delegate_.GetActions());
}

// Tests that suspend is retried on failure.
TEST_F(SuspenderTest, RetryOnFailure) {
  Init();

  const uint64 kOrigWakeupCount = 46;
  delegate_.set_wakeup_count(kOrigWakeupCount);
  suspender_.RequestSuspend();
  suspender_.OnReadyForSuspend(test_api_.suspend_id());
  EXPECT_EQ(kSuspend, delegate_.GetActions());
  EXPECT_EQ(kOrigWakeupCount, delegate_.suspend_wakeup_count());
  EXPECT_TRUE(delegate_.suspend_wakeup_count_valid());

  // Pass a result of 1 in the "on" signal to tell powerd that the suspend was
  // unsuccessful.  The retry timeout should be set and should cause
  // powerd_suspend to be run again.
  suspender_.HandlePowerStateChanged(
      Suspender::kMemState, 0, test_api_.suspend_id());
  suspender_.HandlePowerStateChanged(
      Suspender::kOnState, 1, test_api_.suspend_id());
  EXPECT_EQ(kResume, delegate_.GetActions());

  const uint64 kRetryWakeupCount = 67;
  delegate_.set_wakeup_count(kRetryWakeupCount);
  EXPECT_TRUE(test_api_.TriggerRetryTimeout());
  EXPECT_EQ(kSuspend, delegate_.GetActions());
  EXPECT_EQ(kRetryWakeupCount, delegate_.suspend_wakeup_count());
  EXPECT_TRUE(delegate_.suspend_wakeup_count_valid());

  // Report success this time and check that the timeout is removed.
  suspender_.HandlePowerStateChanged(
      Suspender::kMemState, 0, test_api_.suspend_id());
  suspender_.HandlePowerStateChanged(
      Suspender::kOnState, 0, test_api_.suspend_id());
  EXPECT_FALSE(test_api_.TriggerRetryTimeout());
  EXPECT_EQ(kResume, delegate_.GetActions());
}

// Tests that the system is shut down after repeated suspend failures.
TEST_F(SuspenderTest, ShutdownAfterRepeatedFailures) {
  pref_num_retries_ = 5;
  Init();

  suspender_.RequestSuspend();
  suspender_.OnReadyForSuspend(test_api_.suspend_id());
  EXPECT_EQ(kSuspend, delegate_.GetActions());

  // Proceed through all retries, reporting failure each time.
  for (int i = 1; i <= pref_num_retries_; ++i) {
  suspender_.HandlePowerStateChanged(
      Suspender::kMemState, 0, test_api_.suspend_id());
  suspender_.HandlePowerStateChanged(
      Suspender::kOnState, -1, test_api_.suspend_id());
    EXPECT_EQ(kResume, delegate_.GetActions()) << "Retry #" << (i - 1);
    EXPECT_TRUE(test_api_.TriggerRetryTimeout()) << "Retry #" << i;
    EXPECT_EQ(kSuspend, delegate_.GetActions()) << "Retry #" << i;
  }

  // On the last failed attempt, the system should shut down.
  suspender_.HandlePowerStateChanged(
      Suspender::kMemState, 0, test_api_.suspend_id());
  suspender_.HandlePowerStateChanged(
      Suspender::kOnState, -1, test_api_.suspend_id());
  EXPECT_EQ(kResume, delegate_.GetActions());
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
  suspender_.HandleUserActivity();
  EXPECT_EQ(kEmit, delegate_.GetActions());
  suspender_.OnReadyForSuspend(test_api_.suspend_id());
  EXPECT_EQ(kNoActions, delegate_.GetActions());
  EXPECT_FALSE(test_api_.TriggerRetryTimeout());

  // The lid being opened should also cancel.
  suspender_.RequestSuspend();
  suspender_.HandleLidOpened();
  EXPECT_EQ(kEmit, delegate_.GetActions());
  suspender_.OnReadyForSuspend(test_api_.suspend_id());
  EXPECT_EQ(kNoActions, delegate_.GetActions());
  EXPECT_FALSE(test_api_.TriggerRetryTimeout());

  // The request should also be canceled if the system starts shutting down.
  suspender_.RequestSuspend();
  suspender_.HandleShutdown();
  EXPECT_EQ(kEmit, delegate_.GetActions());
  suspender_.OnReadyForSuspend(test_api_.suspend_id());
  EXPECT_EQ(kNoActions, delegate_.GetActions());
  EXPECT_FALSE(test_api_.TriggerRetryTimeout());
}

// Tests that the delagate's Cancel() method is called if the suspend request
// should be canceled after powerd_suspend has already started.
TEST_F(SuspenderTest, CancelDuringSuspend) {
  Init();

  suspender_.RequestSuspend();
  suspender_.OnReadyForSuspend(test_api_.suspend_id());
  EXPECT_EQ(kSuspend, delegate_.GetActions());
  suspender_.HandlePowerStateChanged(
      Suspender::kMemState, 0, test_api_.suspend_id());
  suspender_.HandleUserActivity();
  EXPECT_EQ(kCancel, delegate_.GetActions());
  suspender_.HandlePowerStateChanged(
      Suspender::kOnState, -1, test_api_.suspend_id());
  EXPECT_EQ(kResume, delegate_.GetActions());
  EXPECT_FALSE(test_api_.TriggerRetryTimeout());

  suspender_.RequestSuspend();
  suspender_.OnReadyForSuspend(test_api_.suspend_id());
  EXPECT_EQ(kSuspend, delegate_.GetActions());
  suspender_.HandlePowerStateChanged(
      Suspender::kMemState, 0, test_api_.suspend_id());
  suspender_.HandleLidOpened();
  EXPECT_EQ(kCancel, delegate_.GetActions());
  suspender_.HandlePowerStateChanged(
      Suspender::kOnState, -1, test_api_.suspend_id());
  EXPECT_FALSE(test_api_.TriggerRetryTimeout());
}

// Tests that the system re-suspends when the lid is quickly opened and
// closed, which can result in lid-open and lid-closed events arriving
// before notification that the system has resumed from the initial
// suspend.
TEST_F(SuspenderTest, NextSuspendRequestArrivesBeforeResume) {
  Init();

  // Request suspend.
  suspender_.RequestSuspend();
  int orig_suspend_id = test_api_.suspend_id();
  suspender_.OnReadyForSuspend(orig_suspend_id);
  EXPECT_EQ(kSuspend, delegate_.GetActions());
  EXPECT_EQ(orig_suspend_id, delegate_.suspend_suspend_id());
  suspender_.HandlePowerStateChanged(Suspender::kMemState, 0, orig_suspend_id);

  // Suppose that the system suspends successfully here.  The user opens
  // the lid, which wakes the system up, but the lid-open event may
  // actually arrive before powerd_suspend's PowerStateChanged "on" signal
  // (sent post-resume after the original suspend).
  suspender_.HandleLidOpened();
  EXPECT_EQ(kCancel, delegate_.GetActions());

  // Before the "on" signal arrives, request suspend again (e.g. from
  // closing the lid immediately).
  suspender_.RequestSuspend();
  int next_suspend_id = test_api_.suspend_id();
  ASSERT_NE(orig_suspend_id, next_suspend_id);

  // Finally delivery the "on" signal.  This resume should be ignored since
  // the system is about to suspend again.
  suspender_.HandlePowerStateChanged(Suspender::kOnState, 0, orig_suspend_id);
  EXPECT_EQ(kNoActions, delegate_.GetActions());

  // When the second suspend request is ready, the system should suspend.
  suspender_.OnReadyForSuspend(next_suspend_id);
  EXPECT_EQ(kSuspend, delegate_.GetActions());
  EXPECT_EQ(next_suspend_id, delegate_.suspend_suspend_id());

  // Resume should be announced this time.
  suspender_.HandlePowerStateChanged(Suspender::kMemState, 0, next_suspend_id);
  suspender_.HandlePowerStateChanged(Suspender::kOnState, 0, next_suspend_id);
  EXPECT_EQ(kResume, delegate_.GetActions());
}

// Tests that Chrome-reported user activity received while suspending with
// a closed lid doesn't abort the suspend attempt (http://crosbug.com/38819).
TEST_F(SuspenderTest, DontCancelForUserActivityWhileLidClosed) {
  delegate_.set_lid_closed(true);
  Init();

  // Report user activity before powerd_suspend is executed and check that
  // Suspender still suspends when OnReadyForSuspend() is called.
  suspender_.RequestSuspend();
  suspender_.HandleUserActivity();
  suspender_.OnReadyForSuspend(test_api_.suspend_id());
  EXPECT_EQ(kSuspend, delegate_.GetActions());
  suspender_.HandlePowerStateChanged(
      Suspender::kMemState, 0, test_api_.suspend_id());
  suspender_.HandlePowerStateChanged(
      Suspender::kOnState, 0, test_api_.suspend_id());
  EXPECT_EQ(kResume, delegate_.GetActions());

  // Report user activity after powerd_suspend and check that the
  // delegate's CancelSuspend() method isn't called.
  suspender_.RequestSuspend();
  suspender_.OnReadyForSuspend(test_api_.suspend_id());
  EXPECT_EQ(kSuspend, delegate_.GetActions());
  suspender_.HandleUserActivity();
  EXPECT_EQ(kNoActions, delegate_.GetActions());
}

}  // namespace power_manager
