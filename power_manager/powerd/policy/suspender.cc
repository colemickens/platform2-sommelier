// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/policy/suspender.h"

#include <algorithm>

#include "base/file_util.h"
#include "base/logging.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/common/clock.h"
#include "power_manager/common/dbus_sender.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/common/prefs.h"
#include "power_manager/common/util.h"
#include "power_manager/powerd/policy/dark_resume_policy.h"
#include "power_manager/powerd/policy/suspend_delay_controller.h"
#include "power_manager/powerd/system/input.h"
#include "power_manager/proto_bindings/suspend.pb.h"

namespace power_manager {
namespace policy {

Suspender::TestApi::TestApi(Suspender* suspender) : suspender_(suspender) {}

void Suspender::TestApi::SetCurrentWallTime(base::Time wall_time) {
  suspender_->clock_->set_current_wall_time_for_testing(wall_time);
}

bool Suspender::TestApi::TriggerRetryTimeout() {
  if (!suspender_->retry_suspend_timer_.IsRunning())
    return false;

  suspender_->retry_suspend_timer_.Stop();
  suspender_->RetrySuspend();
  return true;
}

Suspender::Suspender()
    : delegate_(NULL),
      dbus_sender_(NULL),
      dark_resume_policy_(NULL),
      clock_(new Clock),
      waiting_for_readiness_(false),
      suspend_id_(0),
      wakeup_count_(0),
      wakeup_count_valid_(false),
      got_external_wakeup_count_(false),
      max_retries_(0),
      num_attempts_(0),
      shutting_down_(false) {
}

Suspender::~Suspender() {
}

void Suspender::Init(Delegate *delegate,
                     DBusSenderInterface *dbus_sender,
                     DarkResumePolicy *dark_resume_policy,
                     PrefsInterface *prefs) {
  delegate_ = delegate;
  dbus_sender_ = dbus_sender;
  dark_resume_policy_ = dark_resume_policy;

  suspend_delay_controller_.reset(new SuspendDelayController(dbus_sender_));
  suspend_delay_controller_->AddObserver(this);

  int64 retry_delay_ms = 0;
  CHECK(prefs->GetInt64(kRetrySuspendMsPref, &retry_delay_ms));
  retry_delay_ = base::TimeDelta::FromMilliseconds(retry_delay_ms);

  CHECK(prefs->GetInt64(kRetrySuspendAttemptsPref, &max_retries_));
}

void Suspender::RequestSuspend() {
  got_external_wakeup_count_ = false;
  StartSuspendAttempt();
}

void Suspender::RequestSuspendWithExternalWakeupCount(uint64 wakeup_count) {
  wakeup_count_ = wakeup_count;
  wakeup_count_valid_ = true;
  got_external_wakeup_count_ = true;
  StartSuspendAttempt();
}

void Suspender::RegisterSuspendDelay(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  RegisterSuspendDelayRequest request;
  dbus::MessageReader reader(method_call);
  if (!reader.PopArrayOfBytesAsProto(&request)) {
    LOG(ERROR) << "Unable to parse " << kRegisterSuspendDelayMethod
               << " request";
    response_sender.Run(scoped_ptr<dbus::Response>(
        dbus::ErrorResponse::FromMethodCall(method_call,
            DBUS_ERROR_INVALID_ARGS, "Expected serialized protocol buffer")));
    return;
  }
  RegisterSuspendDelayReply reply_proto;
  suspend_delay_controller_->RegisterSuspendDelay(
      request, method_call->GetSender(), &reply_proto);

  scoped_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  writer.AppendProtoAsArrayOfBytes(reply_proto);
  response_sender.Run(response.Pass());
}

void Suspender::UnregisterSuspendDelay(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  UnregisterSuspendDelayRequest request;
  dbus::MessageReader reader(method_call);
  if (!reader.PopArrayOfBytesAsProto(&request)) {
    LOG(ERROR) << "Unable to parse " << kUnregisterSuspendDelayMethod
               << " request";
    response_sender.Run(scoped_ptr<dbus::Response>(
        dbus::ErrorResponse::FromMethodCall(method_call,
            DBUS_ERROR_INVALID_ARGS, "Expected serialized protocol buffer")));
    return;
  }
  suspend_delay_controller_->UnregisterSuspendDelay(
      request, method_call->GetSender());
  response_sender.Run(dbus::Response::FromMethodCall(method_call));
}

void Suspender::HandleSuspendReadiness(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  SuspendReadinessInfo info;
  dbus::MessageReader reader(method_call);
  if (!reader.PopArrayOfBytesAsProto(&info)) {
    LOG(ERROR) << "Unable to parse " << kHandleSuspendReadinessMethod
               << " request";
    response_sender.Run(scoped_ptr<dbus::Response>(
        dbus::ErrorResponse::FromMethodCall(method_call,
            DBUS_ERROR_INVALID_ARGS, "Expected serialized protocol buffer")));
    return;
  }
  suspend_delay_controller_->HandleSuspendReadiness(
      info, method_call->GetSender());
  response_sender.Run(dbus::Response::FromMethodCall(method_call));
}

void Suspender::HandleLidOpened() {
  CancelSuspend();
}

void Suspender::HandleUserActivity() {
  // Avoid canceling suspend for errant touchpad, power button, etc. events
  // that can be generated by closing the lid.
  if (!delegate_->IsLidClosed())
    CancelSuspend();
}

void Suspender::HandleShutdown() {
  shutting_down_ = true;
  CancelSuspend();
}

void Suspender::HandleDBusNameOwnerChanged(const std::string& name,
                                           const std::string& old_owner,
                                           const std::string& new_owner) {
  if (new_owner.empty())
    suspend_delay_controller_->HandleDBusClientDisconnected(name);
}

void Suspender::OnReadyForSuspend(int suspend_id) {
  if (waiting_for_readiness_ && suspend_id == suspend_id_) {
    LOG(INFO) << "Ready to suspend";
    waiting_for_readiness_ = false;
    Suspend();
  }
}

void Suspender::StartSuspendAttempt() {
  // Suspend shouldn't be requested after the system has started shutting
  // down, but if it is, avoid doing anything.
  if (shutting_down_) {
    LOG(ERROR) << "Not starting suspend attempt; shutdown in progress";
    return;
  }

  if (waiting_for_readiness_)
    return;

  retry_suspend_timer_.Stop();
  if (!got_external_wakeup_count_)
    wakeup_count_valid_ = delegate_->GetWakeupCount(&wakeup_count_);

  suspend_id_++;
  num_attempts_++;
  waiting_for_readiness_ = true;
  delegate_->PrepareForSuspendAnnouncement();
  suspend_delay_controller_->PrepareForSuspend(suspend_id_);
}

void Suspender::Suspend() {
  // Note: If this log message is changed, the power_AudioDetector test
  // must be updated.
  LOG(INFO) << "Starting suspend";

  bool dark_resume = false;
  Delegate::SuspendResult result = Delegate::SUSPEND_SUCCESSFUL;
  bool success = false;

  SendSuspendStateChangedSignal(
      SuspendState_Type_SUSPEND_TO_MEMORY, clock_->GetCurrentWallTime());
  delegate_->PrepareForSuspend();

  do {
    base::TimeDelta suspend_duration;
    DarkResumePolicy::Action action = dark_resume_policy_->GetAction();
    switch (action) {
      case DarkResumePolicy::SHUT_DOWN:
        LOG(INFO) << "Shutting down from dark resume";
        delegate_->ShutDownForDarkResume();
        return;
      case DarkResumePolicy::SUSPEND_FOR_DURATION:
        suspend_duration = dark_resume_policy_->GetSuspendDuration();
        LOG(INFO) << "Suspending for " << suspend_duration.InSeconds()
                  << " seconds";
        break;
      case DarkResumePolicy::SUSPEND_INDEFINITELY:
        suspend_duration = base::TimeDelta();
        break;
      default:
        NOTREACHED() << "Unhandled dark resume action " << action;
    }

    // We don't want to use the wakeup_count in the case of a dark resume. The
    // kernel may not have initialized some of the devices to make the dark
    // resume as inconspicuous as possible, so allowing the user to use the
    // system in this state would be bad.
    result = delegate_->Suspend(
        wakeup_count_, wakeup_count_valid_ && !dark_resume, suspend_duration);
    success = result == Delegate::SUSPEND_SUCCESSFUL;
    dark_resume = dark_resume_policy_->CurrentlyInDarkResume();

    // Failure handling for dark resume. We don't want to process events during
    // a dark resume, even if we fail to suspend. To solve this, instead of
    // scheduling a retry later, delay here and retry without returning from
    // this function.
    if (!success && dark_resume) {
      if (ShutDownIfRetryLimitReached())
        return;
      LOG(WARNING) << "Retry #" << num_attempts_ << " from dark resume";
      sleep(retry_delay_.InSeconds());
      num_attempts_++;
    }
  } while (dark_resume);

  // Don't retry if an external wakeup count was supplied and the suspend
  // attempt failed due to a wakeup count mismatch -- a test probably triggered
  // this suspend attempt after setting a wake alarm, and if we retry later,
  // it's likely that the alarm will have already fired and the system will
  // never wake up.
  const bool done = success ||
      (got_external_wakeup_count_ && result == Delegate::SUSPEND_CANCELED);
  if (done) {
    if (success) {
      LOG(INFO) << "Resumed successfully from suspend attempt " << suspend_id_;
    } else {
      LOG(WARNING) << "Giving up after canceled suspend attempt with external "
                   << "wakeup count";
    }
    SendSuspendStateChangedSignal(
        SuspendState_Type_RESUME, clock_->GetCurrentWallTime());
  } else {
    LOG(INFO) << "Suspend attempt " << suspend_id_ << " failed; "
              << "will retry in " << retry_delay_.InMilliseconds() << " ms";
    retry_suspend_timer_.Start(FROM_HERE, retry_delay_, this,
                               &Suspender::RetrySuspend);
  }

  dark_resume_policy_->HandleResume();
  delegate_->HandleSuspendAttemptCompletion(success, num_attempts_);
  if (done)
    num_attempts_ = 0;
}

bool Suspender::ShutDownIfRetryLimitReached() {
  if (num_attempts_ > max_retries_) {
    LOG(ERROR) << "Unsuccessfully attempted to suspend " << num_attempts_
               << " times; shutting down";
    delegate_->ShutDownForFailedSuspend();
    return true;
  }
  return false;
}

void Suspender::RetrySuspend() {
  if (ShutDownIfRetryLimitReached())
    return;

  LOG(WARNING) << "Retry #" << num_attempts_;
  StartSuspendAttempt();
}

void Suspender::CancelSuspend() {
  if (waiting_for_readiness_) {
    LOG(INFO) << "Canceling suspend before running powerd_suspend";
    waiting_for_readiness_ = false;
    DCHECK(!retry_suspend_timer_.IsRunning());
    delegate_->HandleCanceledSuspendAnnouncement();
  } else if (retry_suspend_timer_.IsRunning()) {
    LOG(INFO) << "Canceling suspend between retries";
    retry_suspend_timer_.Stop();
  }

  if (num_attempts_) {
    delegate_->HandleCanceledSuspendRequest(num_attempts_);
    num_attempts_ = 0;
  }
}

void Suspender::SendSuspendStateChangedSignal(SuspendState_Type type,
                                              const base::Time& wall_time) {
  SuspendState proto;
  proto.set_type(type);
  proto.set_wall_time(wall_time.ToInternalValue());
  dbus_sender_->EmitSignalWithProtocolBuffer(kSuspendStateChangedSignal, proto);
}

}  // namespace policy
}  // namespace power_manager
