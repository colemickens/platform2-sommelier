// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dbus/dbus-shared.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/vt.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/wait.h>

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/stringprintf.h"
#include "chromeos/dbus/dbus.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/common/dbus_sender.h"
#include "power_manager/common/util.h"
#include "power_manager/common/util_dbus.h"
#include "power_manager/input_event.pb.h"
#include "power_manager/powerm/powerman.h"

using base::TimeDelta;
using base::TimeTicks;
using std::string;

namespace power_manager {

PowerManDaemon::PowerManDaemon(PowerPrefs* prefs,
                               MetricsLibraryInterface* metrics_lib,
                               const FilePath& run_dir)
    : loop_(NULL),
      prefs_(prefs),
      metrics_lib_(metrics_lib),
      retry_suspend_count_(0),
      suspend_pid_(0),
      run_dir_(run_dir),
      console_fd_(-1),
      retry_suspend_timeout_id_(0),
      cancel_suspend_if_lid_open_(true),
      dbus_sender_(new DBusSender(kRootPowerManagerServicePath,
                                  kRootPowerManagerInterface)) {}

PowerManDaemon::~PowerManDaemon() {
  if (console_fd_ >= 0)
    close(console_fd_);
  util::RemoveTimeout(&retry_suspend_timeout_id_);
}

void PowerManDaemon::Init() {
  RegisterDBusMessageHandler();
  CHECK(prefs_->GetInt64(kRetrySuspendMsPref, &retry_suspend_ms_));
  CHECK(prefs_->GetInt64(kRetrySuspendAttemptsPref, &retry_suspend_attempts_));
  // Retrys will occur no more than once per 10 seconds.
  CHECK_GE(retry_suspend_ms_, 10000);
  // Only 1-10 retries prior to just shutting down.
  CHECK_GT(retry_suspend_attempts_, 0);
  CHECK_LE(retry_suspend_attempts_, 10);
  loop_ = g_main_loop_new(NULL, false);
  CHECK(GetConsole());
}

void PowerManDaemon::Run() {
  g_main_loop_run(loop_);
}

gboolean PowerManDaemon::RetrySuspend() {
  retry_suspend_timeout_id_ = 0;

  retry_suspend_count_++;
  if (retry_suspend_count_ > retry_suspend_attempts_) {
    LOG(ERROR) << "Retry suspend attempts failed ... shutting down";
    Shutdown(kShutdownReasonSuspendFailed);
    return FALSE;
  }

  LOG(WARNING) << "Retry suspend " << retry_suspend_count_;
  unsigned int wakeup_count = 0;
  if (!util::GetWakeupCount(&wakeup_count)) {
    LOG(ERROR) << "Could not get wakeup count retrying suspend";
    Suspend(0, false, cancel_suspend_if_lid_open_);
  } else {
    Suspend(wakeup_count, true, cancel_suspend_if_lid_open_);
  }
  return FALSE;
}

bool PowerManDaemon::HandleSuspendSignal(DBusMessage* message) {
  unsigned int wakeup_count = 0;
  dbus_bool_t wakeup_count_valid = FALSE;
  dbus_bool_t cancel_if_lid_open = FALSE;

  DBusError error;
  dbus_error_init(&error);
  if (dbus_message_get_args(message, &error,
                            DBUS_TYPE_UINT32, &wakeup_count,
                            DBUS_TYPE_BOOLEAN, &wakeup_count_valid,
                            DBUS_TYPE_BOOLEAN, &cancel_if_lid_open,
                            DBUS_TYPE_INVALID)) {
    Suspend(wakeup_count, wakeup_count_valid, cancel_if_lid_open);
  } else {
    LOG(ERROR) << "Suspend message had invalid arguments: "
               << error.name << " (" << error.message << ")";
    dbus_error_free(&error);
  }

  return true;
}

bool PowerManDaemon::HandleShutdownSignal(DBusMessage* message) {
  const char* reason = '\0';
  DBusError error;
  dbus_error_init(&error);
  if (dbus_message_get_args(message, &error,
                            DBUS_TYPE_STRING, &reason,
                            DBUS_TYPE_INVALID) == FALSE) {
    reason = kShutdownReasonUnknown;
    dbus_error_free(&error);
  }
  Shutdown(reason);
  return true;
}

bool PowerManDaemon::HandleRestartSignal(DBusMessage*) {  // NOLINT
  Restart();
  return true;
}

bool PowerManDaemon::HandleRequestCleanShutdownSignal(DBusMessage*) { // NOLINT
  util::Launch("initctl emit power-manager-clean-shutdown");
  return true;
}

bool PowerManDaemon::HandlePowerStateChangedSignal(DBusMessage* message) {
  const char* state = '\0';
  int32 power_rc = -1;
  DBusError error;
  dbus_error_init(&error);
  if (dbus_message_get_args(message, &error,
                            DBUS_TYPE_STRING, &state,
                            DBUS_TYPE_INT32, &power_rc,
                            DBUS_TYPE_INVALID)) {
    // on == resume via powerd_suspend
    if (strcmp(state, "on") == 0) {
      LOG(INFO) << "Resuming has commenced";
      if (power_rc == 0) {
        GenerateMetricsOnResumeEvent();
        util::RemoveTimeout(&retry_suspend_timeout_id_);
        retry_suspend_count_ = 0;
      } else {
        LOG(INFO) << "Suspend attempt failed";
      }
#ifdef SUSPEND_LOCK_VT
      UnlockVTSwitch();     // Allow virtual terminal switching again.
#endif
      SendSuspendStateChangedSignal(
          SuspendState_Type_RESUME, base::Time::Now());
    } else if (strcmp(state, "mem") == 0) {
      SendSuspendStateChangedSignal(
          SuspendState_Type_SUSPEND_TO_MEMORY, last_suspend_wall_time_);
    } else {
      DLOG(INFO) << "Saw arg:" << state << " for " << kPowerStateChanged;
    }
  } else {
    LOG(WARNING) << "Unable to read " << kPowerStateChanged << " args";
    dbus_error_free(&error);
  }
  return true;
}

void PowerManDaemon::RegisterDBusMessageHandler() {
  util::RequestDBusServiceName(kRootPowerManagerServiceName);

  dbus_handler_.AddDBusSignalHandler(
      kRootPowerManagerInterface,
      kSuspendSignal,
      base::Bind(&PowerManDaemon::HandleSuspendSignal, base::Unretained(this)));
  dbus_handler_.AddDBusSignalHandler(
      kRootPowerManagerInterface,
      kShutdownSignal,
      base::Bind(&PowerManDaemon::HandleShutdownSignal,
                 base::Unretained(this)));
  dbus_handler_.AddDBusSignalHandler(
      kRootPowerManagerInterface,
      kRestartSignal,
      base::Bind(&PowerManDaemon::HandleRestartSignal, base::Unretained(this)));
  dbus_handler_.AddDBusSignalHandler(
      kRootPowerManagerInterface,
      kRequestCleanShutdown,
      base::Bind(&PowerManDaemon::HandleRequestCleanShutdownSignal,
                 base::Unretained(this)));
  dbus_handler_.AddDBusSignalHandler(
      kPowerManagerInterface,
      kPowerStateChanged,
      base::Bind(&PowerManDaemon::HandlePowerStateChangedSignal,
                 base::Unretained(this)));

  dbus_handler_.Start();
}

void PowerManDaemon::SendSuspendStateChangedSignal(
    SuspendState_Type type,
    const base::Time& wall_time) {
  SuspendState proto;
  proto.set_type(type);
  proto.set_wall_time(wall_time.ToInternalValue());
  dbus_sender_->EmitSignalWithProtocolBuffer(kSuspendStateChangedSignal, proto);
}

void PowerManDaemon::Shutdown(const std::string& reason) {
  std::string command = "initctl emit --no-wait runlevel RUNLEVEL=0";
  if (!reason.empty())
    command += std::string(" SHUTDOWN_REASON=") + reason;
  util::Launch(command.c_str());
}

void PowerManDaemon::Restart() {
  util::Launch("shutdown -r now");
}

void PowerManDaemon::Suspend(unsigned int wakeup_count,
                             bool wakeup_count_valid,
                             bool cancel_if_lid_open) {
  LOG(INFO) << "Launching Suspend";
  if ((suspend_pid_ > 0) && !kill(-suspend_pid_, 0)) {
    LOG(ERROR) << "Previous retry suspend pid:"
               << suspend_pid_ << " is still running";
  }

  util::RemoveTimeout(&retry_suspend_timeout_id_);
  cancel_suspend_if_lid_open_ = cancel_if_lid_open;
  retry_suspend_timeout_id_ =
      g_timeout_add(retry_suspend_ms_, RetrySuspendThunk, this);

#ifdef SUSPEND_LOCK_VT
  LockVTSwitch();     // Do not let suspend change the console terminal.
#endif

  // Cache the current time so we can include it in the SuspendStateChanged
  // signal that we emit from HandlePowerStateChangedSignal() -- we might not
  // send it until after the system has already resumed.
  last_suspend_wall_time_ = base::Time::Now();

  std::string suspend_command = "powerd_suspend";
  if (wakeup_count_valid)
    suspend_command += StringPrintf(" --wakeup_count %d", wakeup_count);
  if (cancel_if_lid_open)
    suspend_command += " --cancel_if_lid_open";
  LOG(INFO) << "Running \"" << suspend_command << "\"";

  // Detach to allow suspend to be retried and metrics gathered
  pid_t pid = fork();
  if (pid == 0) {
    setsid();
    if (fork() == 0) {
      wait(NULL);
      exit(system(suspend_command.c_str()));
    } else {
      exit(0);
    }
  } else if (pid > 0) {
    suspend_pid_ = pid;
    waitpid(pid, NULL, 0);
  } else {
    LOG(ERROR) << "Fork for suspend failed";
  }
}

void PowerManDaemon::LockVTSwitch() {
  CHECK_GE(console_fd_, 0);
  if (ioctl(console_fd_, VT_LOCKSWITCH))
    LOG(ERROR) << "Error in ioctl(VT_LOCKSWITCH): " << errno;
  else
    LOG(INFO) << "Invoked ioctl(VT_LOCKSWITCH)";
}

void PowerManDaemon::UnlockVTSwitch() {
  CHECK_GE(console_fd_, 0);
  if (ioctl(console_fd_, VT_UNLOCKSWITCH))
    LOG(ERROR) << "Error in ioctl(VT_UNLOCKSWITCH): " << errno;
  else
    LOG(INFO) << "Invoked ioctl(VT_UNLOCKSWITCH)";
}

bool PowerManDaemon::GetConsole() {
  bool result = true;
  FilePath file_path("/dev/tty0");
  if ((console_fd_ = open(file_path.value().c_str(), O_RDWR)) < 0) {
    LOG(ERROR) << "Failed to open " << file_path.value().c_str()
               << ", errno = " << errno;
    result = false;
  } else {
    LOG(INFO) << "Opened console " << file_path.value().c_str()
              << " with file id = " << console_fd_;
  }
  return result;
}

}  // namespace power_manager
