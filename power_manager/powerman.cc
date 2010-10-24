// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <stdio.h>
#include <dbus/dbus-shared.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/vt.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <gdk/gdkx.h>

#include "base/file_util.h"
#include "base/string_util.h"
#include "chromeos/dbus/dbus.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/power_constants.h"
#include "power_manager/powerman.h"
#include "power_manager/util.h"

namespace power_manager {

static const int kCheckLidClosedSeconds = 10;

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
    suspend_pid_(0),
    lid_id_(0),
    powerd_id_(0),
    powerd_state_(kPowerManagerUnknown),
    console_fd_(-1) {}

PowerManDaemon::~PowerManDaemon() {
  if (console_fd_ >= 0) {
    close(console_fd_);
  }
}

void PowerManDaemon::Init() {
  int input_lidstate = 0;
  CHECK(prefs_->GetInt64(kRetrySuspendMs, &retry_suspend_ms_));
  CHECK(prefs_->GetInt64(kRetrySuspendAttempts, &retry_suspend_attempts_));
  // Retrys will occur no more than once a minute.
  CHECK_GE(retry_suspend_ms_, 60000);
  // Only 1-10 retries prior to just shutting down.
  CHECK_GT(retry_suspend_attempts_, 0);
  CHECK_LE(retry_suspend_attempts_, 10);
  loop_ = g_main_loop_new(NULL, false);
  input_.RegisterHandler(&(PowerManDaemon::OnInputEvent), this);
  CHECK(input_.Init(use_input_for_lid_, use_input_for_key_power_))
    <<"Cannot initialize input interface";
  if (use_input_for_lid_) {
    input_.QueryLidState(&input_lidstate);
    lidstate_ = GetLidState(input_lidstate);
    LOG(INFO) << "PowerM Daemon Init - lid "
              << (lidstate_ == kLidClosed?"closed.":"opened.");
    if (kLidClosed == lidstate_) {
      LOG(INFO) << "PowerM Daemon Init - lid is closed; generating event";
      OnInputEvent(this, LID, input_lidstate);
    }
  }
  RegisterDBusMessageHandler();
  // Attempt to acquire a handle to the console.
  CHECK(GetConsole());
}

void PowerManDaemon::Run() {
  g_main_loop_run(loop_);
}

gboolean PowerManDaemon::CheckLidClosed(gpointer object) {
  RetrySuspendPayload *payload = static_cast<RetrySuspendPayload*>(object);
  PowerManDaemon* daemon = payload->daemon;
  // same lid closed event and powerd state has changed.
  if ((daemon->lidstate_ == kLidClosed) &&
      (daemon->lid_id_ == payload->lid_id) &&
      ((daemon->powerd_state_ != kPowerManagerAlive) ||
       (daemon->powerd_id_ != payload->powerd_id))) {
    LOG(INFO) << "Forced suspend, powerd unstable with pending suspend";
    daemon->Suspend();
  }
  // lid closed events will re-trigger if necessary so always false return.
  delete(payload);
  return false;
}

gboolean PowerManDaemon::RetrySuspend(gpointer object) {
  RetrySuspendPayload *payload = static_cast<RetrySuspendPayload*>(object);
  PowerManDaemon* daemon = payload->daemon;
  if (daemon->lidstate_ == kLidClosed) {
    if (daemon->lid_id_ == payload->lid_id) {
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
    DLOG(INFO) << "Retry suspend  ... lid is open";
  }
  delete(payload);
  return false;
}

void PowerManDaemon::OnInputEvent(void* object, InputType type, int value) {
  PowerManDaemon* daemon = static_cast<PowerManDaemon*>(object);
  switch (type) {
    case LID: {
      daemon->lidstate_ = daemon->GetLidState(value);
      daemon->lid_id_++;
      LOG(INFO) << "PowerM Daemon - lid "
                << (daemon->lidstate_ == kLidClosed?"closed.":"opened.")
                << " powerd "
                << (daemon->powerd_state_ == kPowerManagerDead?
                    "dead":(daemon->powerd_state_ == kPowerManagerAlive?
                            "alive":"unknown"));
      if (daemon->lidstate_ == kLidClosed) {
        if (daemon->powerd_state_ != kPowerManagerAlive) {
          // powerd is not alive so act on its behalf.
          LOG(INFO) << "Forced suspend, powerd is not alive";
          daemon->Suspend();
        } else {
          util::SendSignalToPowerD(util::kLidClosed);
          // Check that powerd stuck around to act on  this event.  If not,
          // callback will assume suspend responsibilities.
          RetrySuspendPayload* payload = daemon->CreateRetrySuspendPayload();
          g_timeout_add_seconds(kCheckLidClosedSeconds,
                                &(PowerManDaemon::CheckLidClosed), payload);
        }
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
    daemon->Shutdown();
  } else if (dbus_message_is_signal(message, util::kLowerPowerManagerInterface,
                                    util::kRestartSignal)) {
    LOG(INFO) << "Restart event";
    daemon->Restart();
  } else if (dbus_message_is_signal(message, util::kLowerPowerManagerInterface,
                                    util::kRequestCleanShutdown)) {
    LOG(INFO) << "Request Clean Shutdown";
    util::Launch("initctl emit power-manager-clean-shutdown");
  } else if (dbus_message_is_signal(message, kPowerManagerInterface,
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
#ifdef SUSPEND_LOCK_VT
      daemon->UnlockVTSwitch();     // Allow virtual terminal switching again.
#endif
    } else {
      DLOG(INFO) << "Saw arg:" << state << " for PowerStateChange";
    }

    // Other dbus clients may be interested in consuming this signal.
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  } else {
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }
  return DBUS_HANDLER_RESULT_HANDLED;
}

void PowerManDaemon::DBusNameOwnerChangedHandler(
    DBusGProxy*, const gchar* name, const gchar* old_owner,
    const gchar* new_owner, void *data) {
  PowerManDaemon* daemon = static_cast<PowerManDaemon*>(data);
  if (strcmp(name, kPowerManagerInterface) == 0) {
    DLOG(INFO) << "name:" << name << " old_owner:" << old_owner
               << " new_owner:" << new_owner;
    daemon->powerd_id_++;
    if (strlen(new_owner) == 0) {
      daemon->powerd_state_ = kPowerManagerDead;
      LOG(WARNING) << "Powerd has stopped";
    } else if (strlen(old_owner) == 0) {
      daemon->powerd_state_ = kPowerManagerAlive;
      LOG(INFO) << "Powerd has started";
    } else {
      daemon->powerd_state_ = kPowerManagerUnknown;
      LOG(WARNING) << "Unrecognized DBus NameOwnerChanged transition of powerd";
    }
  }
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
  AddDBusMatch(connection, kPowerManagerInterface);
  CHECK(dbus_connection_add_filter(
      connection, &DBusMessageHandler, this, NULL));

  DBusGProxy* proxy = dbus_g_proxy_new_for_name(
      chromeos::dbus::GetSystemBusConnection().g_connection(),
      DBUS_SERVICE_DBUS, DBUS_PATH_DBUS, DBUS_INTERFACE_DBUS);

  if (NULL == proxy) {
    LOG(ERROR) << "Failed to connect to freedesktop dbus server.";
    NOTREACHED();
  }
  dbus_g_proxy_add_signal(proxy, "NameOwnerChanged",G_TYPE_STRING,
                          G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
  dbus_g_proxy_connect_signal(proxy, "NameOwnerChanged",
                              G_CALLBACK(DBusNameOwnerChangedHandler),
                              this, NULL);
  LOG(INFO) << "DBus monitoring started";
}

void PowerManDaemon::Shutdown() {
  util::Launch("shutdown -P now");
}

void PowerManDaemon::Restart() {
  util::Launch("shutdown -r now");
}

PowerManDaemon::RetrySuspendPayload* PowerManDaemon::CreateRetrySuspendPayload() {
  RetrySuspendPayload* payload = new RetrySuspendPayload;
  payload->lid_id = lid_id_;
  payload->powerd_id = powerd_id_;
  payload->daemon = this;
  return payload;
}

void PowerManDaemon::Suspend() {
  LOG(INFO) << "Launching Suspend";

  if ((suspend_pid_ > 0) && !kill(-suspend_pid_, 0)) {
    LOG(ERROR) << "Previous retry suspend pid:"
               << suspend_pid_ << " is still running";
  }

  RetrySuspendPayload* payload = CreateRetrySuspendPayload();
  g_timeout_add_seconds(
      retry_suspend_ms_ / 1000, &(PowerManDaemon::RetrySuspend),
      payload);

#ifdef SUSPEND_LOCK_VT
  LockVTSwitch();     // Do not let suspend change the console terminal.
#endif

  // Detach to allow suspend to be retried and metrics gathered.
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

void PowerManDaemon::LockVTSwitch() {
  CHECK(console_fd_ >= 0);
  if (ioctl(console_fd_, VT_LOCKSWITCH))
    LOG(ERROR) << "Error in ioctl(VT_LOCKSWITCH): " << errno;
  else
    LOG(INFO) << "Invoked ioctl(VT_LOCKSWITCH)";
}

void PowerManDaemon::UnlockVTSwitch() {
  CHECK(console_fd_ >= 0);
  if (ioctl(console_fd_, VT_UNLOCKSWITCH))
    LOG(ERROR) << "Error in ioctl(VT_UNLOCKSWITCH): " << errno;
  else
    LOG(INFO) << "Invoked ioctl(VT_UNLOCKSWITCH)";
}

bool PowerManDaemon::GetConsole() {
  bool retval = true;
  FilePath file_path("/dev/tty0");
  if ((console_fd_ = open(file_path.value().c_str(), O_RDWR)) < 0) {
    LOG(ERROR) << "Failed to open " << file_path.value().c_str()
               << ", errno = " << errno;
    retval = false;
  }
  else {
    LOG(INFO) << "Opened console " << file_path.value().c_str()
              << " with file id = " << console_fd_;
  }
  return retval;
}

}  // namespace power_manager
