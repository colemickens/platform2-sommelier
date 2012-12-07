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
#include "base/string_split.h"
#include "base/string_util.h"
#include "chromeos/dbus/dbus.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/common/backlight_interface.h"
#include "power_manager/common/dbus_sender.h"
#include "power_manager/common/util.h"
#include "power_manager/common/util_dbus.h"
#include "power_manager/input_event.pb.h"
#include "power_manager/powerm/powerman.h"

using base::TimeDelta;
using base::TimeTicks;
using std::string;

namespace {

const int kCheckLidClosedSeconds = 10;
const unsigned int kCancelDBusLidOpenedSecs = 5;

}  // namespace

namespace power_manager {

PowerManDaemon::PowerManDaemon(PowerPrefs* prefs,
                               MetricsLibraryInterface* metrics_lib,
                               BacklightInterface* display_backlight,
                               BacklightInterface* keyboard_backlight,
                               const FilePath& run_dir)
    : loop_(NULL),
      use_input_for_lid_(1),
      prefs_(prefs),
      lidstate_(LID_STATE_OPENED),
      metrics_lib_(metrics_lib),
      display_backlight_(display_backlight),
      keyboard_backlight_(keyboard_backlight),
      retry_suspend_count_(0),
      suspend_pid_(0),
      lid_id_(0),
      powerd_id_(0),
      session_state_(SESSION_MANAGER_STOPPED),
      powerd_state_(POWER_MANAGER_UNKNOWN),
      run_dir_(run_dir),
      console_fd_(-1),
      dbus_sender_(new DBusSender(kRootPowerManagerServicePath,
                                  kRootPowerManagerInterface)) {}

PowerManDaemon::~PowerManDaemon() {
  if (console_fd_ >= 0) {
    close(console_fd_);
  }
}

void PowerManDaemon::Init() {
  int input_lidstate = 0;
  int64 use_input_for_lid;
  string wakeup_inputs_str;
  std::vector<string> wakeup_inputs;

  if (prefs_->GetString(kWakeupInputPref, &wakeup_inputs_str))
    base::SplitString(wakeup_inputs_str, '\n', &wakeup_inputs);
  CHECK(prefs_->GetInt64(kRetrySuspendMsPref, &retry_suspend_ms_));
  CHECK(prefs_->GetInt64(kRetrySuspendAttemptsPref, &retry_suspend_attempts_));
  CHECK(prefs_->GetInt64(kUseLidPref, &use_input_for_lid));
  // Retrys will occur no more than once per 10 seconds.
  CHECK_GE(retry_suspend_ms_, 10000);
  // Only 1-10 retries prior to just shutting down.
  CHECK_GT(retry_suspend_attempts_, 0);
  CHECK_LE(retry_suspend_attempts_, 10);
  use_input_for_lid_ = (use_input_for_lid == 1);
  loop_ = g_main_loop_new(NULL, false);
  // Acquire a handle to the console for VT switch locking ioctl.
  CHECK(GetConsole());
  input_.RegisterHandler(&(PowerManDaemon::OnInputEvent), this);
  CHECK(input_.Init(wakeup_inputs)) << "Cannot initialize input interface.";
  lid_open_file_ = FilePath(run_dir_).Append(kLidOpenFile);
  if (input_.num_lid_events() > 0) {
    input_.QueryLidState(&input_lidstate);
    lidstate_ = GetLidState(input_lidstate);
    lid_ticks_ = TimeTicks::Now();
    LOG(INFO) << "PowerM Daemon Init - lid "
              << (lidstate_ == LID_STATE_CLOSED ? "closed." : "opened.");
    if (lidstate_ == LID_STATE_CLOSED) {
      SetTouchDevices(false);
      input_.DisableWakeInputs();
      LOG(INFO) << "PowerM Daemon Init - lid is closed; generating event";
      OnInputEvent(this, INPUT_LID, input_lidstate);
    } else {
      SetTouchDevices(true);
      input_.EnableWakeInputs();
    }
  }
  RegisterDBusMessageHandler();
}

void PowerManDaemon::Run() {
  g_main_loop_run(loop_);
}

gboolean PowerManDaemon::CheckLidClosed(unsigned int lid_id,
                                        unsigned int powerd_id) {
  unsigned int wakeup_count;
  // Same lid closed event and powerd state has changed.
  if ((lidstate_ == LID_STATE_CLOSED) && (lid_id_ == lid_id) &&
      ((powerd_state_ != POWER_MANAGER_ALIVE) || (powerd_id_ != powerd_id))) {
    LOG(INFO) << "Forced suspend, powerd unstable with pending suspend";
    if (!util::GetWakeupCount(&wakeup_count)) {
      LOG(ERROR) << "Could not get wakeup count trying to suspend";
      Suspend();
    } else {
      Suspend(wakeup_count);
    }
  }
  // lid closed events will re-trigger if necessary so always false return.
  return false;
}

gboolean PowerManDaemon::RetrySuspend(unsigned int lid_id) {
  unsigned int wakeup_count;

  if (lidstate_ == LID_STATE_CLOSED) {
    if (lid_id_ == lid_id) {
      retry_suspend_count_++;
      if (retry_suspend_count_ > retry_suspend_attempts_) {
        LOG(ERROR) << "Retry suspend attempts failed ... shutting down";
        Shutdown(kShutdownReasonSuspendFailed);
      } else {
        LOG(WARNING) << "Retry suspend " << retry_suspend_count_;
        if (!util::GetWakeupCount(&wakeup_count)) {
          LOG(ERROR) << "Could not get wakeup count retrying suspend";
          Suspend();
        } else {
          Suspend(wakeup_count);
        }
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
    case INPUT_LID: {
      daemon->lidstate_ = daemon->GetLidState(value);
      daemon->lid_id_++;
      daemon->lid_ticks_ = TimeTicks::Now();
      LOG(INFO) << "PowerM Daemon - lid "
                << (daemon->lidstate_ == LID_STATE_CLOSED ?
                    "closed." : "opened.")
                << " powerd "
                << (daemon->powerd_state_ == POWER_MANAGER_DEAD ?
                    "dead" : (daemon->powerd_state_ == POWER_MANAGER_ALIVE ?
                              "alive" : "unknown"))
                << ". session "
                << (daemon->session_state_ == SESSION_MANAGER_STARTED ?
                    "started." : "stopped");
      if (!daemon->use_input_for_lid_) {
        LOG(INFO) << "Ignoring lid.";
        break;
      }
      if (daemon->lidstate_ == LID_STATE_CLOSED) {
        daemon->SetTouchDevices(false);
        daemon->input_.DisableWakeInputs();
        daemon->SendInputEventSignal(INPUT_LID, BUTTON_DOWN);
        // Check that powerd stuck around to act on  this event.  If not,
        // callback will assume suspend responsibilities.
        g_timeout_add_seconds(kCheckLidClosedSeconds, CheckLidClosedThunk,
                              CreateCheckLidClosedArgs(daemon,
                                                       daemon->lid_id_,
                                                       daemon->powerd_id_));
      } else {
        daemon->SetTouchDevices(true);
        daemon->input_.EnableWakeInputs();
        util::CreateStatusFile(daemon->lid_open_file_);
        daemon->SendInputEventSignal(INPUT_LID, BUTTON_UP);
      }
      break;
    }
    case INPUT_POWER_BUTTON:
      daemon->SendInputEventSignal(type, GetButtonState(value));
      break;
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
  LOG(INFO) << (cancel ? "Canceled" : "Continuing")
            << " DBus activated suspend.  Lid is "
            << (lidstate_ == LID_STATE_CLOSED ? "closed." : "open.");
  return cancel;
}

BacklightInterface* PowerManDaemon::GetBacklight(BacklightType type) {
  switch (type) {
    case BACKLIGHT_TYPE_DISPLAY:
      return display_backlight_;
    case BACKLIGHT_TYPE_KEYBOARD:
      return keyboard_backlight_;
    default:
      LOG(ERROR) << "Unknown backlight type " << type;
      return NULL;
  }
}

bool PowerManDaemon::HandleCheckLidStateSignal(DBusMessage*) {  // NOLINT
  if (lidstate_ == LID_STATE_CLOSED)
    SendInputEventSignal(INPUT_LID, BUTTON_DOWN);
  return true;
}

bool PowerManDaemon::HandleSuspendSignal(DBusMessage* message) {
  Suspend(message);
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

bool PowerManDaemon::HandleSessionManagerStateChangedSignal(
    DBusMessage* message) {
  DBusError error;
  dbus_error_init(&error);
  const char* state = NULL;
  const char* user = NULL;
  if (dbus_message_get_args(message, &error,
                            DBUS_TYPE_STRING, &state,
                            DBUS_TYPE_STRING, &user,
                            DBUS_TYPE_INVALID)) {
    if (strcmp(state, "started") == 0)
      session_state_ = SESSION_MANAGER_STARTED;
    else if (strcmp(state, "stopping") == 0)
      session_state_ = SESSION_MANAGER_STOPPING;
    else if (strcmp(state, "stopped") == 0)
      session_state_ = SESSION_MANAGER_STOPPED;
    else
      LOG(WARNING) << "Unknown session state : " << state;
  } else {
    LOG(WARNING) << "Unable to read "
                 << login_manager::kSessionManagerSessionStateChanged
                 << " args";
    dbus_error_free(&error);
  }
  return true;
}

DBusMessage* PowerManDaemon::HandleBacklightGetMethod(DBusMessage* message) {
  BacklightType type = BACKLIGHT_TYPE_DISPLAY;
  DBusError error;
  dbus_error_init(&error);
  if (!dbus_message_get_args(message, &error,
                             DBUS_TYPE_INT32, &type,
                             DBUS_TYPE_INVALID)) {
    LOG(WARNING) << "Unable to read " << kBacklightGetMethod << " args";
    dbus_error_free(&error);
    return NULL;
  }

  BacklightInterface* backlight = GetBacklight(type);
  if (!backlight) {
    LOG(WARNING) << "Ignoring " << kBacklightGetMethod << " request for "
                 << "nonexistent backlight of type " << type;
    return NULL;
  }

  int64 current_level = 0;
  int64 max_level = 0;
  dbus_bool_t result = backlight->GetCurrentBrightnessLevel(&current_level) &&
                       backlight->GetMaxBrightnessLevel(&max_level);

  DBusMessage* reply = util::CreateEmptyDBusReply(message);
  dbus_message_append_args(reply,
                           DBUS_TYPE_INT64, &current_level,
                           DBUS_TYPE_INT64, &max_level,
                           DBUS_TYPE_BOOLEAN, &result,
                           DBUS_TYPE_INVALID);
  return reply;
}

DBusMessage* PowerManDaemon::HandleBacklightSetMethod(DBusMessage* message) {
  BacklightType type = BACKLIGHT_TYPE_DISPLAY;
  int64 level = 0;
  int64 interval_internal = 0;
  DBusError error;
  dbus_error_init(&error);
  if (!dbus_message_get_args(message, &error,
                             DBUS_TYPE_INT32, &type,
                             DBUS_TYPE_INT64, &level,
                             DBUS_TYPE_INT64, &interval_internal,
                             DBUS_TYPE_INVALID)) {
    LOG(WARNING) << "Unable to read " << kBacklightSetMethod << " args";
    dbus_error_free(&error);
    return NULL;
  }

  BacklightInterface* backlight = GetBacklight(type);
  if (backlight) {
    backlight->SetBrightnessLevel(
        level, base::TimeDelta::FromInternalValue(interval_internal));
  } else {
    LOG(WARNING) << "Ignoring " << kBacklightSetMethod << " request for "
                 << "nonexistent backlight of type " << type;
  }
  return NULL;
}

void PowerManDaemon::DBusNameOwnerChangedHandler(
    DBusGProxy*, const gchar* name, const gchar* old_owner,
    const gchar* new_owner, void* data) {
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
      daemon->powerd_state_ = POWER_MANAGER_DEAD;
      LOG(WARNING) << "Powerd has stopped";
    } else if (strlen(old_owner) == 0) {
      daemon->powerd_state_ = POWER_MANAGER_ALIVE;
      LOG(INFO) << "Powerd has started";
      if (daemon->use_input_for_lid_ &&
          daemon->lidstate_ == LID_STATE_CLOSED) {
        LOG(INFO) << "Lid is closed. Sending message to powerd on respawn.";
        daemon->SendInputEventSignal(INPUT_LID, BUTTON_DOWN);
      }
    } else {
      daemon->powerd_state_ = POWER_MANAGER_UNKNOWN;
      LOG(WARNING) << "Unrecognized DBus NameOwnerChanged transition of powerd";
    }
  }
}

void PowerManDaemon::RegisterDBusMessageHandler() {
  util::RequestDBusServiceName(kRootPowerManagerServiceName);

  dbus_handler_.AddDBusSignalHandler(
      kRootPowerManagerInterface,
      kCheckLidStateSignal,
      base::Bind(&PowerManDaemon::HandleCheckLidStateSignal,
                 base::Unretained(this)));
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
  dbus_handler_.AddDBusSignalHandler(
      login_manager::kSessionManagerInterface,
      login_manager::kSessionManagerSessionStateChanged,
      base::Bind(&PowerManDaemon::HandleSessionManagerStateChangedSignal,
                 base::Unretained(this)));

  dbus_handler_.AddDBusMethodHandler(
      kRootPowerManagerInterface,
      kBacklightGetMethod,
      base::Bind(&PowerManDaemon::HandleBacklightGetMethod,
                 base::Unretained(this)));
  dbus_handler_.AddDBusMethodHandler(
      kRootPowerManagerInterface,
      kBacklightSetMethod,
      base::Bind(&PowerManDaemon::HandleBacklightSetMethod,
                 base::Unretained(this)));

  dbus_handler_.Start();

  util::SetNameOwnerChangedHandler(DBusNameOwnerChangedHandler, this);
}

void PowerManDaemon::SendInputEventSignal(InputType type, ButtonState state) {
  if (state == BUTTON_REPEAT)
    return;
  LOG(INFO) << "Sending input event signal: " << util::InputTypeToString(type)
            << " is " << (state == BUTTON_UP ? "up" : "down");

  InputEvent proto;
  switch (type) {
    case INPUT_LID:
      proto.set_type(state == BUTTON_DOWN ?
                     InputEvent_Type_LID_CLOSED :
                     InputEvent_Type_LID_OPEN);
      break;
    case INPUT_POWER_BUTTON:
      proto.set_type(state == BUTTON_DOWN ?
                     InputEvent_Type_POWER_BUTTON_DOWN :
                     InputEvent_Type_POWER_BUTTON_UP);
      break;
    default:
      NOTREACHED() << "Unhandled input event type " << type;
  }
  proto.set_timestamp(base::TimeTicks::Now().ToInternalValue());
  dbus_sender_->EmitSignalWithProtocolBuffer(kInputEventSignal, proto);

  // TODO(derat): Remove this after Chrome is using the protocol-buffer-based
  // signal above.
  dbus_int32_t type_param = type;
  dbus_bool_t down_param = (state == BUTTON_DOWN) ? TRUE : FALSE;
  dbus_int64_t timestamp_param = base::TimeTicks::Now().ToInternalValue();
  chromeos::dbus::Proxy proxy(chromeos::dbus::GetSystemBusConnection(),
                              kPowerManagerServicePath,
                              kPowerManagerInterface);
  DBusMessage* signal = dbus_message_new_signal(kPowerManagerServicePath,
                                                kPowerManagerInterface,
                                                kInputEventSignal);
  CHECK(signal);
  dbus_message_append_args(signal,
                           DBUS_TYPE_INT32, &type_param,
                           DBUS_TYPE_BOOLEAN, &down_param,
                           DBUS_TYPE_INT64, &timestamp_param,
                           DBUS_TYPE_INVALID);
  dbus_g_proxy_send(proxy.gproxy(), signal, NULL);
  dbus_message_unref(signal);
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
                             bool wakeup_count_valid) {
  LOG(INFO) << "Launching Suspend";
  if ((suspend_pid_ > 0) && !kill(-suspend_pid_, 0)) {
    LOG(ERROR) << "Previous retry suspend pid:"
               << suspend_pid_ << " is still running";
  }
  g_timeout_add_seconds(retry_suspend_ms_ / 1000, RetrySuspendThunk,
                        CreateRetrySuspendArgs(this, lid_id_));

  // create command line
  char suspend_command[60];
  if (wakeup_count_valid && snprintf(suspend_command, sizeof(suspend_command),
                                     "powerd_suspend --wakeup_count %d",
                                     wakeup_count) == sizeof(suspend_command)) {
    LOG(ERROR) << "Command line exceeded size limit: "
               << sizeof(suspend_command);
    // We shouldn't ever exceed 60 chars (leaves 40 chars for the int)
    // If we do, exit out and let the retry handle it
    return;
  }

#ifdef SUSPEND_LOCK_VT
  LockVTSwitch();     // Do not let suspend change the console terminal.
#endif

  // Cache the current time so we can include it in the SuspendStateChanged
  // signal that we emit from HandlePowerStateChangedSignal() -- we might not
  // send it until after the system has already resumed.
  last_suspend_wall_time_ = base::Time::Now();

  // Remove lid opened flag, so suspend will occur providing the lid isn't
  // re-opened prior to completing powerd_suspend
  util::RemoveStatusFile(lid_open_file_);
  // Detach to allow suspend to be retried and metrics gathered
  pid_t pid = fork();
  if (pid == 0) {
    setsid();
    if (fork() == 0) {
      wait(NULL);
      if (CancelDBusRequest()) {
        exit(system("powerd_suspend --cancel"));
      } else if (wakeup_count_valid) {
        exit(system(suspend_command));
      } else {
        exit(system("powerd_suspend"));
      }
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

void PowerManDaemon::Suspend() {
  Suspend(0, false);
}

void PowerManDaemon::Suspend(unsigned int wakeup_count) {
  Suspend(wakeup_count, true);
}

void PowerManDaemon::Suspend(DBusMessage* message) {
  unsigned int wakeup_count;
  DBusError error;
  dbus_error_init(&error);
  if (!dbus_message_get_args(message, &error,
                             DBUS_TYPE_UINT32, &wakeup_count,
                             DBUS_TYPE_INVALID)) {
    LOG(ERROR) << "Suspend message missing wakeup_count: "
               << error.name << " (" << error.message << ")";
    dbus_error_free(&error);
    Suspend();
  } else {
    Suspend(wakeup_count);
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

void PowerManDaemon::SetTouchDevices(bool enable) {
#ifdef TOUCH_DEVICE
  if (enable) {
    if (lidstate_ == LID_STATE_CLOSED) {
      // Do not allow the touchscreen to be enabled when the lid is closed.
      return;
    }
    util::Launch("/opt/google/touch/touch-control.sh --enable");
  } else {
    util::Run("/opt/google/touch/touch-control.sh --disable");
  }
#endif // TOUCH_DEVICE
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
