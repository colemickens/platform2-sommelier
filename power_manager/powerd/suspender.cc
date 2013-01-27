// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/suspender.h"

#include <algorithm>

#include "base/bind.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "chromeos/dbus/dbus.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/common/dbus_sender.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/common/prefs.h"
#include "power_manager/common/util.h"
#include "power_manager/common/util_dbus.h"
#include "power_manager/powerd/file_tagger.h"
#include "power_manager/powerd/powerd.h"
#include "power_manager/powerd/suspend_delay_controller.h"
#include "power_manager/powerd/system/input.h"
#include "power_manager/suspend.pb.h"

using std::max;
using std::min;

namespace power_manager {

Suspender::Suspender(Daemon* daemon,
                     FileTagger* file_tagger,
                     DBusSenderInterface* dbus_sender,
                     system::Input* input,
                     const FilePath& run_dir)
    : daemon_(daemon),
      file_tagger_(file_tagger),
      dbus_sender_(dbus_sender),
      input_(input),
      suspend_delay_controller_(new SuspendDelayController(dbus_sender)),
      suspend_requested_(false),
      suspend_id_(0),
      cancel_suspend_if_lid_open_(true),
      user_active_file_(run_dir.Append(kUserActiveFile)),
      wakeup_count_valid_(false),
      num_retries_(0),
      retry_suspend_timeout_id_(0) {
  suspend_delay_controller_->AddObserver(this);
}

Suspender::~Suspender() {
  suspend_delay_controller_->RemoveObserver(this);
  util::RemoveTimeout(&retry_suspend_timeout_id_);
}

void Suspender::NameOwnerChangedHandler(DBusGProxy*,
                                        const gchar* name,
                                        const gchar* /*old_owner*/,
                                        const gchar* new_owner,
                                        gpointer data) {
  Suspender* suspender = static_cast<Suspender*>(data);
  if (name && new_owner && strlen(new_owner) == 0)
    suspender->suspend_delay_controller_->HandleDBusClientDisconnected(name);
}

void Suspender::Init(PrefsInterface* prefs) {
  int64 retry_delay_ms = 0;
  CHECK(prefs->GetInt64(kRetrySuspendMsPref, &retry_delay_ms));
  retry_delay_ = base::TimeDelta::FromMilliseconds(retry_delay_ms);

  CHECK(prefs->GetInt64(kRetrySuspendAttemptsPref, &max_retries_));
}

void Suspender::RequestSuspend(bool cancel_if_lid_open) {
  suspend_requested_ = true;
  cancel_suspend_if_lid_open_ = cancel_if_lid_open;
  wakeup_count_ = 0;
  wakeup_count_valid_ = util::GetWakeupCount(&wakeup_count_);
  if (!wakeup_count_valid_)
    LOG(ERROR) << "Could not get wakeup_count prior to suspend.";

  suspend_id_++;
  suspend_delay_controller_->PrepareForSuspend(suspend_id_);
}

void Suspender::SuspendIfReady() {
  if (suspend_requested_ &&
      suspend_delay_controller_->ready_for_suspend()) {
    suspend_requested_ = false;
    LOG(INFO) << "Ready to suspend; suspending";
    Suspend();
  }
}

void Suspender::CancelSuspend() {
  if (suspend_requested_) {
    LOG(INFO) << "Suspend canceled mid flight.";
    daemon_->ResumePollPowerSupply();

    // Send a PowerStateChanged "on" signal when suspend is canceled.
    //
    // TODO(benchan): Refactor this code and the code in the powerd_suspend
    // script.
    chromeos::dbus::Proxy proxy(chromeos::dbus::GetSystemBusConnection(),
                                kPowerManagerServicePath,
                                kPowerManagerInterface);
    DBusMessage* signal = dbus_message_new_signal(kPowerManagerServicePath,
                                                  kPowerManagerInterface,
                                                  kPowerStateChanged);
    const char* power_state = "on";
    int32 suspend_rc = -1;
    dbus_message_append_args(signal, DBUS_TYPE_STRING, &power_state,
                             DBUS_TYPE_INT32, &suspend_rc,
                             DBUS_TYPE_INVALID);
    dbus_g_proxy_send(proxy.gproxy(), signal, NULL);
    dbus_message_unref(signal);
  }

  suspend_requested_ = false;
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

void Suspender::HandlePowerStateChanged(const char* state, int power_rc) {
  // on == resume via powerd_suspend
  if (strcmp(state, "on") == 0) {
    LOG(INFO) << "Resuming has commenced";
    if (power_rc == 0) {
      util::RemoveTimeout(&retry_suspend_timeout_id_);
      daemon_->GenerateRetrySuspendMetric(num_retries_, max_retries_);
      num_retries_ = 0;
    } else {
      LOG(INFO) << "Suspend attempt failed";
    }
#ifdef SUSPEND_LOCK_VT
    // Allow virtual terminal switching again.
    util::RunSetuidHelper("unlock_vt", "", true);
#endif
    SendSuspendStateChangedSignal(
        SuspendState_Type_RESUME, base::Time::Now());
  } else if (strcmp(state, "mem") == 0) {
    SendSuspendStateChangedSignal(
        SuspendState_Type_SUSPEND_TO_MEMORY, last_suspend_wall_time_);
  } else {
    DLOG(INFO) << "Saw arg:" << state << " for " << kPowerStateChanged;
  }
}

void Suspender::OnReadyForSuspend(int suspend_id) {
  if (suspend_id == suspend_id_)
    SuspendIfReady();
}

void Suspender::Suspend() {
  LOG(INFO) << "Starting suspend";
  daemon_->HaltPollPowerSupply();
  daemon_->MarkPowerStatusStale();
  util::RemoveStatusFile(user_active_file_);
  file_tagger_->HandleSuspendEvent();

  util::RemoveTimeout(&retry_suspend_timeout_id_);
  retry_suspend_timeout_id_ =
      g_timeout_add(retry_delay_.InMilliseconds(), RetrySuspendThunk, this);

#ifdef SUSPEND_LOCK_VT
  // Do not let suspend change the console terminal.
  util::RunSetuidHelper("lock_vt", "", true);
#endif

  // Cache the current time so we can include it in the SuspendStateChanged
  // signal that we emit from HandlePowerStateChangedSignal() -- we might not
  // send it until after the system has already resumed.
  last_suspend_wall_time_ = base::Time::Now();

  std::string args;
  if (wakeup_count_valid_)
    args += StringPrintf(" --suspend_wakeup_count %d", wakeup_count_);
  if (cancel_suspend_if_lid_open_)
    args += " --suspend_cancel_if_lid_open";
  util::RunSetuidHelper("suspend", args, false);
}

gboolean Suspender::RetrySuspend() {
  retry_suspend_timeout_id_ = 0;

  if (num_retries_ >= max_retries_) {
    LOG(ERROR) << "Retried suspend " << num_retries_ << " times; shutting down";
    daemon_->ShutdownForFailedSuspend();
    return FALSE;
  }

  num_retries_++;
  LOG(WARNING) << "Retry suspend attempt #" << num_retries_;
  wakeup_count_valid_ = util::GetWakeupCount(&wakeup_count_);
  Suspend();
  return FALSE;
}

void Suspender::SendSuspendStateChangedSignal(SuspendState_Type type,
                                              const base::Time& wall_time) {
  SuspendState proto;
  proto.set_type(type);
  proto.set_wall_time(wall_time.ToInternalValue());
  dbus_sender_->EmitSignalWithProtocolBuffer(kSuspendStateChangedSignal, proto);
}

}  // namespace power_manager
