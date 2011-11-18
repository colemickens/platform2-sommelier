// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
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

using base::TimeDelta;
using base::TimeTicks;

namespace {

const int kCheckLidClosedSeconds = 10;
const unsigned int kCancelDBusLidOpenedSecs = 5;

}  // namespace

namespace power_manager {

PowerManDaemon::PowerManDaemon(PowerPrefs* prefs,
                               MetricsLibraryInterface* metrics_lib,
                               const FilePath& run_dir)
  : loop_(NULL),
    use_input_for_lid_(1),
    prefs_(prefs),
    lidstate_(LID_STATE_OPENED),
    metrics_lib_(metrics_lib),
    power_button_state_(false),
    retry_suspend_count_(0),
    suspend_pid_(0),
    lid_id_(0),
    powerd_id_(0),
    session_state_(kSessionStopped),
    powerd_state_(kPowerManagerUnknown),
    run_dir_(run_dir),
    console_fd_(-1) {}

PowerManDaemon::~PowerManDaemon() {
  if (console_fd_ >= 0) {
    close(console_fd_);
  }
}

void PowerManDaemon::Init() {
  int input_lidstate = 0;
  int64 use_input_for_lid;
  CHECK(prefs_->GetInt64(kRetrySuspendMs, &retry_suspend_ms_));
  CHECK(prefs_->GetInt64(kRetrySuspendAttempts, &retry_suspend_attempts_));
  CHECK(prefs_->GetInt64(kUseLid, &use_input_for_lid));
  // Retrys will occur no more than once a minute.
  CHECK_GE(retry_suspend_ms_, 60000);
  // Only 1-10 retries prior to just shutting down.
  CHECK_GT(retry_suspend_attempts_, 0);
  CHECK_LE(retry_suspend_attempts_, 10);
  use_input_for_lid_ = 1 == use_input_for_lid;
  loop_ = g_main_loop_new(NULL, false);
  // Acquire a handle to the console for VT switch locking ioctl.
  CHECK(GetConsole());
  input_.RegisterHandler(&(PowerManDaemon::OnInputEvent), this);
  CHECK(input_.Init()) << "Cannot initialize input interface.";
  lid_open_file_ = FilePath(run_dir_).Append(kLidOpenFile);
  if (input_.num_lid_events() > 0) {
    input_.QueryLidState(&input_lidstate);
    lidstate_ = GetLidState(input_lidstate);
    lid_ticks_ = TimeTicks::Now();
    LOG(INFO) << "PowerM Daemon Init - lid "
              << (lidstate_ == LID_STATE_CLOSED ? "closed." : "opened.");
    if (lidstate_ == LID_STATE_CLOSED) {
      LOG(INFO) << "PowerM Daemon Init - lid is closed; generating event";
      OnInputEvent(this, LID, input_lidstate);
    }
  }
  RegisterDBusMessageHandler();
}

void PowerManDaemon::Run() {
  g_main_loop_run(loop_);
}

gboolean PowerManDaemon::CheckLidClosed(unsigned int lid_id,
                                        unsigned int powerd_id) {
  // Same lid closed event and powerd state has changed.
  if ((lidstate_ == LID_STATE_CLOSED) && (lid_id_ == lid_id) &&
      ((powerd_state_ != kPowerManagerAlive) || (powerd_id_ != powerd_id))) {
    LOG(INFO) << "Forced suspend, powerd unstable with pending suspend";
    Suspend();
  }
  // lid closed events will re-trigger if necessary so always false return.
  return false;
}

gboolean PowerManDaemon::RetrySuspend(unsigned int lid_id) {
  if (lidstate_ == LID_STATE_CLOSED) {
    if (lid_id_ == lid_id) {
      retry_suspend_count_++;
      if (retry_suspend_count_ > retry_suspend_attempts_) {
        LOG(ERROR) << "Retry suspend attempts failed ... shutting down";
        Shutdown();
      } else {
        LOG(WARNING) << "Retry suspend " << retry_suspend_count_;
        Suspend();
      }
    } else {
      LOG(INFO) << "Retry suspend sequence number changed, retry delayed";
    }
  } else {
    DLOG(INFO) << "Retry suspend  ... lid is open";
  }
  // Return false so the event trigger does not repeat.
  return false;
}

void PowerManDaemon::OnInputEvent(void* object, InputType type, int value) {
  PowerManDaemon* daemon = static_cast<PowerManDaemon*>(object);
  switch (type) {
    case LID: {
      daemon->lidstate_ = daemon->GetLidState(value);
      daemon->lid_id_++;
      daemon->lid_ticks_ = TimeTicks::Now();
      LOG(INFO) << "PowerM Daemon - lid "
                << (daemon->lidstate_ == LID_STATE_CLOSED ?
                    "closed." : "opened.")
                << " powerd "
                << (daemon->powerd_state_ == kPowerManagerDead ?
                    "dead" : (daemon->powerd_state_ == kPowerManagerAlive ?
                              "alive" : "unknown"))
                << ". session "
                << (daemon->session_state_ == kSessionStarted ?
                    "started." : "stopped");
      if (!daemon->use_input_for_lid_) {
        LOG(INFO) << "Ignoring lid.";
        break;
      }
      if (daemon->lidstate_ == LID_STATE_CLOSED) {
        util::SendSignalToPowerD(kLidClosed);
        // Check that powerd stuck around to act on  this event.  If not,
        // callback will assume suspend responsibilities.
        g_timeout_add_seconds(kCheckLidClosedSeconds, CheckLidClosedThunk,
                              CreateCheckLidClosedArgs(daemon,
                                                       daemon->lid_id_,
                                                       daemon->powerd_id_));
      } else {
        util::CreateStatusFile(daemon->lid_open_file_);
        util::SendSignalToPowerD(kLidOpened);
      }
      break;
    }
    case PWRBUTTON: {
      daemon->power_button_state_ = daemon->GetPowerButtonState(value);
      LOG(INFO) << "PowerM Daemon - power button "
                << (daemon->power_button_state_ == POWER_BUTTON_DOWN ?
                    "down." : "up.");
      if (daemon->power_button_state_ == POWER_BUTTON_DOWN)
        util::SendSignalToPowerD(kPowerButtonDown);
      else
        util::SendSignalToPowerD(kPowerButtonUp);
      break;
    }
    default: {
      LOG(ERROR) << "Bad input type.";
      NOTREACHED();
      break;
    }
  }
}

bool PowerManDaemon::CancelDBusRequest() {
  TimeDelta delta = TimeTicks::Now() - lid_ticks_;

  bool cancel = (lidstate_ == LID_STATE_OPENED) &&
                (delta.InSeconds() < kCancelDBusLidOpenedSecs);
  LOG(INFO) << (cancel?"Canceled":"Continuing")
            << " DBus activated suspend.  Lid is "
            << (lidstate_ == LID_STATE_CLOSED ? "closed." : "open.");
  return cancel;
}

DBusHandlerResult PowerManDaemon::DBusMessageHandler(
    DBusConnection*, DBusMessage* message, void* data) {
  PowerManDaemon* daemon = static_cast<PowerManDaemon*>(data);
  if (dbus_message_is_signal(message, kRootPowerManagerInterface,
                             kSuspendSignal)) {
    LOG(INFO) << "Suspend event";
    daemon->Suspend();
  } else if (dbus_message_is_signal(message, kRootPowerManagerInterface,
                                    kShutdownSignal)) {
    LOG(INFO) << "Shutdown event";
    daemon->Shutdown();
  } else if (dbus_message_is_signal(message, kRootPowerManagerInterface,
                                    kRestartSignal)) {
    LOG(INFO) << "Restart event";
    daemon->Restart();
  } else if (dbus_message_is_signal(message, kRootPowerManagerInterface,
                                    kRequestCleanShutdown)) {
    LOG(INFO) << "Request Clean Shutdown";
    util::Launch("initctl emit power-manager-clean-shutdown");
  } else if (dbus_message_is_signal(message, kPowerManagerInterface,
                                    kPowerStateChanged)) {
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
  } else if (dbus_message_is_signal(
                 message,
                 login_manager::kSessionManagerInterface,
                 login_manager::kSessionManagerSessionStateChanged)) {
    DBusError error;
    dbus_error_init(&error);
    const char* state = NULL;
    const char* user = NULL;
    if (dbus_message_get_args(message, &error,
                              DBUS_TYPE_STRING, &state,
                              DBUS_TYPE_STRING, &user,
                              DBUS_TYPE_INVALID)) {
      if (strcmp(state, "started") == 0)
        daemon->session_state_ = kSessionStarted;
      else if (strcmp(state, "stopping") == 0)
        daemon->session_state_ = kSessionStopping;
      else if (strcmp(state, "stopped") == 0)
        daemon->session_state_ = kSessionStopped;
      else
        LOG(WARNING) << "Unknown session state : " << state;
    } else {
      LOG(WARNING) << "Unable to read arguments from "
                   << login_manager::kSessionManagerSessionStateChanged
                   << " signal";
    }
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
  if (!name || !new_owner || !old_owner) {
    LOG(ERROR) << "NameOwnerChanged with Null name.";
    return;
  }
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
      if (daemon->use_input_for_lid_ &&
          daemon->lidstate_ == LID_STATE_CLOSED) {
        LOG(INFO) << "Lid is closed. Sending message to powerd on respawn.";
        util::SendSignalToPowerD(kLidClosed);
      }
    } else {
      daemon->powerd_state_ = kPowerManagerUnknown;
      LOG(WARNING) << "Unrecognized DBus NameOwnerChanged transition of powerd";
    }
  }
}

void PowerManDaemon::AddDBusMatch(DBusConnection *connection,
                                   const char *interface,
                                   const char *member) {
  DBusError error;
  dbus_error_init(&error);
  std::string filter;
  if (member)
    filter = StringPrintf("type='signal', interface='%s', member='%s'",
                          interface, member);
  else
    filter = StringPrintf("type='signal', interface='%s'", interface);
  dbus_bus_add_match(connection, filter.c_str(), &error);
  if (dbus_error_is_set(&error)) {
    LOG(ERROR) << "Failed to add a match:" << error.name << ", message="
               << error.message;
    NOTREACHED();
  }
}


void PowerManDaemon::AddDBusMatch(DBusConnection *connection,
                                   const char *interface) {
  AddDBusMatch(connection, interface, NULL);
}

void PowerManDaemon::RegisterDBusMessageHandler() {
  DBusConnection* connection = dbus_g_connection_get_connection(
      chromeos::dbus::GetSystemBusConnection().g_connection());
  AddDBusMatch(connection,
               login_manager::kSessionManagerInterface,
               login_manager::kSessionManagerSessionStateChanged);
  AddDBusMatch(connection, kRootPowerManagerInterface);
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
  dbus_g_proxy_add_signal(proxy, "NameOwnerChanged", G_TYPE_STRING,
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

void PowerManDaemon::Suspend() {
  LOG(INFO) << "Launching Suspend";

  if ((suspend_pid_ > 0) && !kill(-suspend_pid_, 0)) {
    LOG(ERROR) << "Previous retry suspend pid:"
               << suspend_pid_ << " is still running";
  }

  g_timeout_add_seconds(retry_suspend_ms_ / 1000, RetrySuspendThunk,
                        CreateRetrySuspendArgs(this, lid_id_));

#ifdef SUSPEND_LOCK_VT
  LockVTSwitch();     // Do not let suspend change the console terminal.
#endif

  // Remove lid opened flag, so suspend will occur providing the lid isn't
  // re-opened prior to completing powerd_suspend
  util::RemoveStatusFile(lid_open_file_);
  // Detach to allow suspend to be retried and metrics gathered
  pid_t pid = fork();
  if (pid == 0) {
    setsid();
    if (fork() == 0) {
      wait(NULL);
      exit(CancelDBusRequest() ? system("powerd_suspend --cancel")
                               : system("powerd_suspend"));
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
