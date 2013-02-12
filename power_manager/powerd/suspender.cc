// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/suspender.h"

#include <inttypes.h>

#include <algorithm>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "chromeos/dbus/dbus.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/common/dbus_sender.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/common/prefs.h"
#include "power_manager/common/util.h"
#include "power_manager/common/util_dbus.h"
#include "power_manager/powerd/powerd.h"
#include "power_manager/powerd/suspend_delay_controller.h"
#include "power_manager/powerd/system/input.h"
#include "power_manager/suspend.pb.h"

namespace power_manager {

namespace {

const char kWakeupCountPath[] = "/sys/power/wakeup_count";

}  // namespace

// Real implementation of the Delegate interface.
class Suspender::RealDelegate : public Suspender::Delegate {
 public:
  RealDelegate(Daemon* daemon, system::Input* input)
      : daemon_(daemon),
        input_(input) {
  }

  virtual ~RealDelegate() {}

  // Delegate implementation:
  virtual bool IsLidClosed() OVERRIDE {
    int lid_state = 0;
    if (!input_->QueryLidState(&lid_state))
      return false;
    return lid_state == 1;
  }

  virtual bool GetWakeupCount(uint64* wakeup_count) OVERRIDE {
    DCHECK(wakeup_count);
    FilePath path(kWakeupCountPath);
    std::string buf;
    if (file_util::ReadFileToString(path, &buf)) {
      TrimWhitespaceASCII(buf, TRIM_TRAILING, &buf);
      if (base::StringToUint64(buf, wakeup_count))
        return true;

      LOG(ERROR) << "Could not parse wakeup count from \"" << buf << "\"";
    } else {
      LOG(ERROR) << "Could not read " << kWakeupCountPath;
    }
    return false;
  }

  virtual bool Suspend(uint64 wakeup_count, bool wakeup_count_valid) {
    daemon_->PrepareForSuspend();

    std::string args;
    if (wakeup_count_valid) {
      args += StringPrintf(" --suspend_wakeup_count_valid"
                           " --suspend_wakeup_count %" PRIu64, wakeup_count);
    }
    return util::RunSetuidHelper("suspend", args, true) == 0;
  }

  virtual void EmitPowerStateChangedOnSignal() OVERRIDE {
    // TODO(benchan): Refactor this code and the code in the powerd_suspend
    // script.
    chromeos::dbus::Proxy proxy(chromeos::dbus::GetSystemBusConnection(),
                                kPowerManagerServicePath,
                                kPowerManagerInterface);
    DBusMessage* signal = dbus_message_new_signal(kPowerManagerServicePath,
                                                  kPowerManagerInterface,
                                                  kPowerStateChanged);
    const char* power_state = "on";
    dbus_message_append_args(signal,
                             DBUS_TYPE_STRING, &power_state,
                             DBUS_TYPE_INVALID);
    dbus_g_proxy_send(proxy.gproxy(), signal, NULL);
    dbus_message_unref(signal);
  }

  virtual void HandleResume(bool suspend_was_successful,
                            int num_suspend_retries,
                            int max_suspend_retries) OVERRIDE {
    daemon_->HandleResume(suspend_was_successful,
                          num_suspend_retries,
                          max_suspend_retries);
  }

  virtual void ShutdownForFailedSuspend() OVERRIDE {
    daemon_->ShutdownForFailedSuspend();
  }

 private:
  Daemon* daemon_;  // not owned
  system::Input* input_;  // not owned

  DISALLOW_COPY_AND_ASSIGN(RealDelegate);
};

Suspender::TestApi::TestApi(Suspender* suspender) : suspender_(suspender) {}

void Suspender::TestApi::SetCurrentWallTime(base::Time wall_time) {
  suspender_->current_wall_time_for_testing_ = wall_time;
}

bool Suspender::TestApi::TriggerRetryTimeout() {
  if (suspender_->retry_suspend_timeout_id_ == 0)
    return false;

  guint old_id = suspender_->retry_suspend_timeout_id_;
  if (!suspender_->RetrySuspend())
    util::RemoveTimeout(&old_id);
  return true;
}

// static
Suspender::Delegate* Suspender::CreateDefaultDelegate(Daemon* daemon,
                                                      system::Input* input) {
  return new RealDelegate(daemon, input);
}

// static
void Suspender::NameOwnerChangedHandler(DBusGProxy*,
                                        const gchar* name,
                                        const gchar* /*old_owner*/,
                                        const gchar* new_owner,
                                        gpointer data) {
  Suspender* suspender = static_cast<Suspender*>(data);
  if (name && new_owner && strlen(new_owner) == 0)
    suspender->suspend_delay_controller_->HandleDBusClientDisconnected(name);
}

Suspender::Suspender(Delegate* delegate,
                     DBusSenderInterface* dbus_sender)
    : delegate_(delegate),
      dbus_sender_(dbus_sender),
      suspend_delay_controller_(new SuspendDelayController(dbus_sender)),
      waiting_for_readiness_(false),
      suspend_id_(0),
      wakeup_count_(0),
      wakeup_count_valid_(false),
      max_retries_(0),
      num_retries_(0),
      retry_suspend_timeout_id_(0) {
  suspend_delay_controller_->AddObserver(this);
}

Suspender::~Suspender() {
  suspend_delay_controller_->RemoveObserver(this);
  util::RemoveTimeout(&retry_suspend_timeout_id_);
}

void Suspender::Init(PrefsInterface* prefs) {
  int64 retry_delay_ms = 0;
  CHECK(prefs->GetInt64(kRetrySuspendMsPref, &retry_delay_ms));
  retry_delay_ = base::TimeDelta::FromMilliseconds(retry_delay_ms);

  CHECK(prefs->GetInt64(kRetrySuspendAttemptsPref, &max_retries_));
}

void Suspender::RequestSuspend() {
  if (waiting_for_readiness_)
    return;

  waiting_for_readiness_ = true;
  util::RemoveTimeout(&retry_suspend_timeout_id_);
  wakeup_count_valid_ = delegate_->GetWakeupCount(&wakeup_count_);
  suspend_id_++;
  suspend_delay_controller_->PrepareForSuspend(suspend_id_);
}

DBusMessage* Suspender::RegisterSuspendDelay(DBusMessage* message) {
  RegisterSuspendDelayRequest request;
  if (!util::ParseProtocolBufferFromDBusMessage(message, &request)) {
    LOG(ERROR) << "Unable to parse RegisterSuspendDelay request";
    return util::CreateDBusInvalidArgsErrorReply(message);
  }
  RegisterSuspendDelayReply reply_proto;
  suspend_delay_controller_->RegisterSuspendDelay(
      request, util::GetDBusSender(message), &reply_proto);
  return util::CreateDBusProtocolBufferReply(message, reply_proto);
}

DBusMessage* Suspender::UnregisterSuspendDelay(DBusMessage* message) {
  UnregisterSuspendDelayRequest request;
  if (!util::ParseProtocolBufferFromDBusMessage(message, &request)) {
    LOG(ERROR) << "Unable to parse UnregisterSuspendDelay request";
    return util::CreateDBusInvalidArgsErrorReply(message);
  }
  suspend_delay_controller_->UnregisterSuspendDelay(
      request, util::GetDBusSender(message));
  return NULL;
}

DBusMessage* Suspender::HandleSuspendReadiness(DBusMessage* message) {
  SuspendReadinessInfo info;
  if (!util::ParseProtocolBufferFromDBusMessage(message, &info)) {
    LOG(ERROR) << "Unable to parse HandleSuspendReadiness request";
    return util::CreateDBusInvalidArgsErrorReply(message);
  }
  suspend_delay_controller_->HandleSuspendReadiness(
      info, util::GetDBusSender(message));
  return NULL;
}

void Suspender::HandleLidOpened() {
  CancelSuspend();
}

void Suspender::HandleUserActivity() {
  if (delegate_->IsLidClosed())
    LOG(INFO) << "Ignoring user activity received while lid is closed";
  else
    CancelSuspend();
}

void Suspender::HandleShutdown() {
  CancelSuspend();
}

void Suspender::OnReadyForSuspend(int suspend_id) {
  if (waiting_for_readiness_ && suspend_id == suspend_id_) {
    LOG(INFO) << "Ready to suspend";
    waiting_for_readiness_ = false;
    Suspend();
  }
}

base::Time Suspender::GetCurrentWallTime() const {
  return !current_wall_time_for_testing_.is_null() ?
      current_wall_time_for_testing_ : base::Time::Now();
}

void Suspender::Suspend() {
  // Note: If this log message is changed, the power_AudioDetector test
  // must be updated.
  LOG(INFO) << "Starting suspend";

  SendSuspendStateChangedSignal(
      SuspendState_Type_SUSPEND_TO_MEMORY, GetCurrentWallTime());

  bool success = delegate_->Suspend(wakeup_count_, wakeup_count_valid_);
  if (success) {
    LOG(INFO) << "Resumed successfully from suspend attempt " << suspend_id_;
    num_retries_ = 0;
    SendSuspendStateChangedSignal(
        SuspendState_Type_RESUME, GetCurrentWallTime());
  } else {
    LOG(INFO) << "Suspend attempt " << suspend_id_ << " failed; "
              << "will retry in " << retry_delay_.InMilliseconds() << " ms";
    DCHECK(!retry_suspend_timeout_id_);
    retry_suspend_timeout_id_ =
        g_timeout_add(retry_delay_.InMilliseconds(), RetrySuspendThunk, this);
  }

  delegate_->HandleResume(success, num_retries_, max_retries_);
}

gboolean Suspender::RetrySuspend() {
  retry_suspend_timeout_id_ = 0;

  if (num_retries_ >= max_retries_) {
    LOG(ERROR) << "Retried suspend " << num_retries_ << " times; shutting down";
    delegate_->ShutdownForFailedSuspend();
  } else {
    num_retries_++;
    LOG(WARNING) << "Retry #" << num_retries_;
    RequestSuspend();
  }
  return FALSE;
}

void Suspender::CancelSuspend() {
  if (waiting_for_readiness_) {
    LOG(INFO) << "Canceling suspend before running powerd_suspend";
    waiting_for_readiness_ = false;
    DCHECK(!retry_suspend_timeout_id_);
    delegate_->EmitPowerStateChangedOnSignal();
  } else if (retry_suspend_timeout_id_) {
    LOG(INFO) << "Canceling suspend between retries";
    util::RemoveTimeout(&retry_suspend_timeout_id_);
  }
}

void Suspender::SendSuspendStateChangedSignal(SuspendState_Type type,
                                              const base::Time& wall_time) {
  SuspendState proto;
  proto.set_type(type);
  proto.set_wall_time(wall_time.ToInternalValue());
  dbus_sender_->EmitSignalWithProtocolBuffer(kSuspendStateChangedSignal, proto);
}

}  // namespace power_manager
