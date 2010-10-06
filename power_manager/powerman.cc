// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/wait.h>
#include <gdk/gdkx.h>

#include "base/string_util.h"
#include "chromeos/dbus/dbus.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/powerman.h"
#include "power_manager/util.h"

namespace power_manager {

PowerManDaemon::PowerManDaemon(bool use_input_for_lid,
                               bool use_input_for_key_power,
                               PowerPrefs* prefs,
                               MetricsLibraryInterface* metrics_lib)
  : loop_(NULL),
    use_input_for_lid_(use_input_for_lid),
    use_input_for_key_power_(use_input_for_key_power),
    prefs_(prefs),
    lidstate_(kLidOpened),
    metrics_lib_(metrics_lib),
    power_button_state_(false),
    retry_suspend_count_(0),
    suspend_sequence_number_(0),
    suspend_pid_(0) {}

void PowerManDaemon::Init() {
  int lid_state = 0;
  CHECK(prefs_->ReadSetting("retry_suspend_ms", &retry_suspend_ms_));
  CHECK(prefs_->ReadSetting("retry_suspend_attempts",
                            &retry_suspend_attempts_));
  // retrys will occur no more than once a minute
  CHECK_GE(retry_suspend_ms_, 60000);
  // only 1-10 retries prior to just shutting down
  CHECK_GT(retry_suspend_attempts_, 0);
  CHECK_LE(retry_suspend_attempts_, 10);
  loop_ = g_main_loop_new(NULL, false);
  CHECK(input_.Init(use_input_for_lid_, use_input_for_key_power_))
    <<"Cannot initialize input interface";
  if (use_input_for_lid_) {
    input_.QueryLidState(&lid_state);
    lidstate_ = GetLidState(lid_state);
    LOG(INFO) << "PowerM Daemon Init - lid "
              << (lidstate_ == kLidClosed?"closed.":"opened.");
  }
  input_.RegisterHandler(&(PowerManDaemon::OnInputEvent), this);
  RegisterDBusMessageHandler();
}

void PowerManDaemon::Run() {
  g_main_loop_run(loop_);
}

gboolean PowerManDaemon::RetrySuspend(gpointer object) {
  RetrySuspendPayload *payload = static_cast<RetrySuspendPayload*>(object);
  PowerManDaemon* daemon = payload->daemon;
  if (daemon->lidstate_ == kLidClosed) {
    if (daemon->suspend_sequence_number_ == payload->suspend_sequence_number) {
      daemon->retry_suspend_count_++;
      if (daemon->retry_suspend_count_ > daemon->retry_suspend_attempts_) {
        LOG(ERROR) << "Retry suspend attempts failed ... shutting down";
        daemon->Shutdown();
      } else {
        LOG(WARNING) << "Retry suspend " << daemon->retry_suspend_count_;
        daemon->Suspend();
      }
    } else {
      LOG(INFO) << "Retry suspend sequence number changed, retry delayed";
    }
  } else {
    daemon->CleanupSuspendRetry();
    LOG(INFO) << "Retry suspend cancelled ... lid is open";
  }
  delete(payload);
  return false;
}

void PowerManDaemon::OnInputEvent(void* object, InputType type, int value) {
  PowerManDaemon* daemon = static_cast<PowerManDaemon*>(object);
  switch (type) {
    case LID: {
      daemon->lidstate_ = daemon->GetLidState(value);
      daemon->suspend_sequence_number_++;
      LOG(INFO) << "PowerM Daemon - lid "
                << (daemon->lidstate_ == kLidClosed?"closed.":"opened.");
      if (daemon->lidstate_ == kLidClosed) {
        util::SendSignalToPowerD(util::kLidClosed);
      } else {
        util::SendSignalToPowerD(util::kLidOpened);
      }
      break;
    }
    case PWRBUTTON: {
      daemon->power_button_state_ = daemon->GetPowerButtonState(value);
      LOG(INFO) << "PowerM Daemon - power button "
                << (daemon->power_button_state_ == kPowerButtonDown?
                    "down.":"up.");
      if (daemon->power_button_state_ == kPowerButtonDown)
        util::SendSignalToPowerD(util::kPowerButtonDown);
      else
        util::SendSignalToPowerD(util::kPowerButtonUp);
      break;
    }
    default: {
      LOG(ERROR) << "Bad input type.";
      NOTREACHED();
      break;
    }
  }
}

DBusHandlerResult PowerManDaemon::DBusMessageHandler(
    DBusConnection*, DBusMessage* message, void* data) {
  PowerManDaemon* daemon = static_cast<PowerManDaemon*>(data);
  if (dbus_message_is_signal(message, util::kLowerPowerManagerInterface,
                             util::kSuspendSignal)) {
    LOG(INFO) << "Suspend event";
    LOG(INFO) << "lid is "
              << (daemon->lidstate_ == kLidClosed?"closed.":"opened.");
    daemon->Suspend();
  } else if (dbus_message_is_signal(message, util::kLowerPowerManagerInterface,
                                    util::kShutdownSignal)) {
    LOG(INFO) << "Shutdown event";
    LOG(INFO) << "power button is "
              << (daemon->power_button_state_ == kPowerButtonDown?
                  "down.":"up.");
    daemon->Shutdown();
  } else if (dbus_message_is_signal(message, util::kLowerPowerManagerInterface,
                                    util::kRequestCleanShutdown)) {
    LOG(INFO) << "Request Clean Shutdown";
    util::Launch("initctl emit power-manager-clean-shutdown");
  } else if (dbus_message_is_signal(message, util::kPowerManagerInterface,
                                    util::kPowerStateChanged)) {
    LOG(INFO) << "Power state change event";
    const char *state = '\0';
    DBusError error;
    dbus_error_init(&error);

    if (dbus_message_get_args(message, &error, DBUS_TYPE_STRING, &state,
                              DBUS_TYPE_INVALID) == FALSE) {
      LOG(WARNING) << "Trouble reading args of PowerStateChange event "
                   << state;
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    // on == resume via powerd_suspend
    if (g_str_equal(state, "on") == TRUE) {
      LOG(INFO) << "Resuming has commenced";
      daemon->GenerateMetricsOnResumeEvent();
      daemon->retry_suspend_count_ = 0;
    } else {
      DLOG(INFO) << "Saw arg:" << state << " for PowerStateChange";
    }

    // other dbus clients may be interested in consuming this signal
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  } else {
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }
  return DBUS_HANDLER_RESULT_HANDLED;
}

void PowerManDaemon::AddDBusMatch(DBusConnection *connection,
                                   const char *interface) {
  DBusError error;
  dbus_error_init(&error);
  const std::string filter = StringPrintf(
      "type='signal', interface='%s'", interface);
  dbus_bus_add_match(connection, filter.c_str(), &error);
  if (dbus_error_is_set(&error)) {
    LOG(ERROR) << "Failed to add a match:" << error.name << ", message="
               << error.message;
    NOTREACHED();
  }
}

void PowerManDaemon::RegisterDBusMessageHandler() {
  DBusConnection* connection = dbus_g_connection_get_connection(
      chromeos::dbus::GetSystemBusConnection().g_connection());
  AddDBusMatch(connection, util::kLowerPowerManagerInterface);
  AddDBusMatch(connection, util::kPowerManagerInterface);
  CHECK(dbus_connection_add_filter(
      connection, &DBusMessageHandler, this, NULL));
  LOG(INFO) << "DBus monitoring started";
}

void PowerManDaemon::Shutdown() {
  util::Launch("shutdown -P now");
}

void PowerManDaemon::CleanupSuspendRetry() {
  // kill any previously attempted suspend thats hanging around
  if ((suspend_pid_ > 0) && !kill(-suspend_pid_, 0)) {
    if (kill(-suspend_pid_, SIGTERM)) {
      LOG(ERROR) << "Error trying to kill previous suspend. pid:"
                 << suspend_pid_ << " error:" << errno;
    }
    waitpid(suspend_pid_, NULL, 0);
    suspend_pid_ = 0;
  }
}

void PowerManDaemon::Suspend() {
  LOG(INFO) << "Launching Suspend";

  CleanupSuspendRetry();

  RetrySuspendPayload* payload = new RetrySuspendPayload;
  payload->daemon = this;
  payload->suspend_sequence_number = suspend_sequence_number_;

  g_timeout_add_seconds(
      retry_suspend_ms_ / 1000, &(PowerManDaemon::RetrySuspend),
      payload);

  // Detach to allow suspend to be retried and metrics gathered
  pid_t pid = fork();
  if (pid == 0) {
    setsid();
    exit(fork() == 0 ? wait(NULL), system("powerd_suspend") : 0);
  } else if (pid > 0) {
    suspend_pid_ = pid;
    waitpid(pid, NULL, 0);
  } else {
    LOG(ERROR) << "Fork for suspend failed";
  }
}

}  // namespace power_manager
