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

#include "base/file_util.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "chromeos/dbus/dbus.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/backlight_interface.h"
#include "power_manager/power_constants.h"
#include "power_manager/powerman.h"
#include "power_manager/util.h"
#include "power_manager/util_dbus.h"

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
                               BacklightInterface* backlight,
                               const FilePath& run_dir)
  : loop_(NULL),
    use_input_for_lid_(1),
    prefs_(prefs),
    lidstate_(LID_STATE_OPENED),
    metrics_lib_(metrics_lib),
    backlight_(backlight),
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
  string wakeup_inputs_str;
  std::vector<string> wakeup_inputs;

  if (prefs_->GetString(kWakeupInputPref, &wakeup_inputs_str))
    base::SplitString(wakeup_inputs_str, '\n', &wakeup_inputs);
  CHECK(prefs_->GetInt64(kRetrySuspendMsPref, &retry_suspend_ms_));
  CHECK(prefs_->GetInt64(kRetrySuspendAttemptsPref, &retry_suspend_attempts_));
  CHECK(prefs_->GetInt64(kUseLidPref, &use_input_for_lid));
  // Retrys will occur no more than once a minute.
  CHECK_GE(retry_suspend_ms_, 60000);
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
      input_.DisableWakeInputs();
      LOG(INFO) << "PowerM Daemon Init - lid is closed; generating event";
      OnInputEvent(this, INPUT_LID, input_lidstate);
    } else {
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
      ((powerd_state_ != kPowerManagerAlive) || (powerd_id_ != powerd_id))) {
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
        Shutdown();
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
        daemon->input_.DisableWakeInputs();
        util::SendSignalToPowerD(kLidClosed);
        // Check that powerd stuck around to act on  this event.  If not,
        // callback will assume suspend responsibilities.
        g_timeout_add_seconds(kCheckLidClosedSeconds, CheckLidClosedThunk,
                              CreateCheckLidClosedArgs(daemon,
                                                       daemon->lid_id_,
                                                       daemon->powerd_id_));
      } else {
        daemon->input_.EnableWakeInputs();
        util::CreateStatusFile(daemon->lid_open_file_);
        util::SendSignalToPowerD(kLidOpened);
      }
      break;
    }
    case INPUT_POWER_BUTTON:
      daemon->HandlePowerButtonEvent(GetButtonState(value));
      break;
    case INPUT_LOCK_BUTTON:
      daemon->SendButtonEventSignal(kLockButtonName, GetButtonState(value));
      break;
    case INPUT_KEY_LEFT_CTRL:
      daemon->SendButtonEventSignal(kKeyLeftCtrl, GetButtonState(value));
      break;
    case INPUT_KEY_RIGHT_CTRL:
      daemon->SendButtonEventSignal(kKeyRightCtrl, GetButtonState(value));
      break;
    case INPUT_KEY_LEFT_ALT:
      daemon->SendButtonEventSignal(kKeyLeftAlt, GetButtonState(value));
      break;
    case INPUT_KEY_RIGHT_ALT:
      daemon->SendButtonEventSignal(kKeyRightAlt, GetButtonState(value));
      break;
    case INPUT_KEY_LEFT_SHIFT:
      daemon->SendButtonEventSignal(kKeyLeftShift, GetButtonState(value));
      break;
    case INPUT_KEY_RIGHT_SHIFT:
      daemon->SendButtonEventSignal(kKeyRightShift, GetButtonState(value));
      break;
    case INPUT_KEY_F4:
      daemon->SendButtonEventSignal(kKeyF4, GetButtonState(value));
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

DBusHandlerResult PowerManDaemon::DBusMessageHandler(
    DBusConnection* connection, DBusMessage* message, void* data) {
  PowerManDaemon* daemon = static_cast<PowerManDaemon*>(data);
  CHECK(daemon);

  // Filter out error messages -- should not be getting them.
  int type = dbus_message_get_type(message);
  if (type == DBUS_MESSAGE_TYPE_ERROR) {
    util::LogDBusError(message);
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  // Look up and call the corresponding dbus message handler.
  std::string interface = dbus_message_get_interface(message);
  std::string member = dbus_message_get_member(message);
  DBusInterfaceMemberPair dbus_message_pair = std::make_pair(interface, member);

  if (type == DBUS_MESSAGE_TYPE_METHOD_CALL) {
    DBusMethodHandlerTable::iterator iter =
        daemon->dbus_method_handler_table_.find(dbus_message_pair);
    if (iter == daemon->dbus_method_handler_table_.end())
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    LOG(INFO) << "Got " << member << " method call";
    DBusMethodHandler callback = iter->second;
    DBusMessage* reply = (daemon->*callback)(message);

    // Must send a reply if it is a message.
    CHECK(reply);
    dbus_connection_send(connection, reply, NULL);
    dbus_message_unref(reply);

    return DBUS_HANDLER_RESULT_HANDLED;
  } else if (type == DBUS_MESSAGE_TYPE_SIGNAL) {
    DBusSignalHandlerTable::iterator iter =
        daemon->dbus_signal_handler_table_.find(dbus_message_pair);
    if (iter == daemon->dbus_signal_handler_table_.end())
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    LOG(INFO) << "Got " << member << " signal";
    DBusSignalHandler callback = iter->second;
    (daemon->*callback)(message);
    // Do not send a reply if it is a signal.
  }
  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

void PowerManDaemon::HandlePowerButtonEvent(ButtonState value) {
  // Forward the signal to be handled by powerd and chrome
  SendButtonEventSignal(kPowerButtonName, value);

  // On button down, since the user may be doing a long press to cause a
  // hardware shutdown, sync our state.
  if (value == BUTTON_DOWN) {
    LOG(INFO) << "Syncing state due to power button down event";
    util::Launch("sync");
  }
}

void PowerManDaemon::HandleCheckLidStateSignal(DBusMessage*) {  // NOLINT
  if (lidstate_ == LID_STATE_CLOSED) {
    util::SendSignalToPowerD(kLidClosed);
  }
}

void PowerManDaemon::HandleSuspendSignal(DBusMessage* message) {
  Suspend(message);
}

void PowerManDaemon::HandleShutdownSignal(DBusMessage*) {  // NOLINT
  Shutdown();
}

void PowerManDaemon::HandleRestartSignal(DBusMessage*) {  // NOLINT
  Restart();
}

void PowerManDaemon::HandleRequestCleanShutdownSignal(DBusMessage*) {  // NOLINT
  util::Launch("initctl emit power-manager-clean-shutdown");
}

void PowerManDaemon::HandlePowerStateChangedSignal(DBusMessage* message) {
  const char* state = '\0';
  DBusError error;
  dbus_error_init(&error);
  if (dbus_message_get_args(message, &error,
                            DBUS_TYPE_STRING, &state,
                            DBUS_TYPE_INVALID)) {
    // on == resume via powerd_suspend
    if (g_str_equal(state, "on") == TRUE) {
      LOG(INFO) << "Resuming has commenced";
      GenerateMetricsOnResumeEvent();
      retry_suspend_count_ = 0;
#ifdef SUSPEND_LOCK_VT
      UnlockVTSwitch();     // Allow virtual terminal switching again.
#endif
    } else {
      DLOG(INFO) << "Saw arg:" << state << " for " << kPowerStateChanged;
    }
  } else {
    LOG(WARNING) << "Unable to read " << kPowerStateChanged << " args";
    dbus_error_free(&error);
  }
}

void PowerManDaemon::HandleSessionManagerStateChangedSignal(
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
      session_state_ = kSessionStarted;
    else if (strcmp(state, "stopping") == 0)
      session_state_ = kSessionStopping;
    else if (strcmp(state, "stopped") == 0)
      session_state_ = kSessionStopped;
    else
      LOG(WARNING) << "Unknown session state : " << state;
  } else {
    LOG(WARNING) << "Unable to read "
                 << login_manager::kSessionManagerSessionStateChanged
                 << " args";
    dbus_error_free(&error);
  }
}

DBusMessage* PowerManDaemon::HandleExternalBacklightGetMethod(
    DBusMessage* message) {
  int64 current_level = 0;
  int64 max_level = 0;
  dbus_bool_t result = FALSE;
  if (backlight_) {
    result = backlight_->GetCurrentBrightnessLevel(&current_level) &&
             backlight_->GetMaxBrightnessLevel(&max_level);
  }

  DBusMessage* reply = dbus_message_new_method_return(message);
  CHECK(reply);
  dbus_message_append_args(reply,
                           DBUS_TYPE_INT64, &current_level,
                           DBUS_TYPE_INT64, &max_level,
                           DBUS_TYPE_BOOLEAN, &result,
                           DBUS_TYPE_INVALID);
  return reply;
}

DBusMessage* PowerManDaemon::HandleExternalBacklightSetMethod(
    DBusMessage* message) {
  int64 level = 0;
  DBusError error;
  dbus_error_init(&error);
  if (dbus_message_get_args(message, &error,
                            DBUS_TYPE_INT64, &level,
                            DBUS_TYPE_INVALID)) {
    if (backlight_)
      backlight_->SetBrightnessLevel(level);
  } else {
    LOG(WARNING) << "Unable to read " << kExternalBacklightSetMethod << " args";
    dbus_error_free(&error);
  }
  return dbus_message_new_method_return(message);
}

void PowerManDaemon::AddDBusSignalHandler(const std::string& interface,
                                          const std::string& member,
                                          DBusSignalHandler handler) {
  DBusConnection* connection = dbus_g_connection_get_connection(
      chromeos::dbus::GetSystemBusConnection().g_connection());
  CHECK(connection);
  util::AddDBusSignalMatch(connection, interface, member);
  dbus_signal_handler_table_[std::make_pair(interface, member)] = handler;
}

void PowerManDaemon::AddDBusMethodHandler(const std::string& interface,
                                          const std::string& member,
                                          DBusMethodHandler handler) {
  DBusConnection* connection = dbus_g_connection_get_connection(
      chromeos::dbus::GetSystemBusConnection().g_connection());
  CHECK(connection);
  util::AddDBusMethodMatch(
      connection, interface, kPowerManagerServicePath, member);
  dbus_method_handler_table_[std::make_pair(interface, member)] = handler;
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

void PowerManDaemon::RegisterDBusMessageHandler() {
  DBusConnection* connection = dbus_g_connection_get_connection(
      chromeos::dbus::GetSystemBusConnection().g_connection());
  CHECK(connection);
  DBusError error;
  dbus_error_init(&error);
  if (dbus_bus_request_name(connection,
                            kRootPowerManagerServiceName,
                            0,
                            &error) < 0) {
    LOG(FATAL) << "Failed to register name \""
               << kRootPowerManagerServiceName << "\": "
               << (dbus_error_is_set(&error) ? error.message : "Unknown error");
  }

  AddDBusSignalHandler(kRootPowerManagerInterface, kCheckLidStateSignal,
                       &PowerManDaemon::HandleCheckLidStateSignal);
  AddDBusSignalHandler(kRootPowerManagerInterface, kSuspendSignal,
                       &PowerManDaemon::HandleSuspendSignal);
  AddDBusSignalHandler(kRootPowerManagerInterface, kShutdownSignal,
                       &PowerManDaemon::HandleShutdownSignal);
  AddDBusSignalHandler(kRootPowerManagerInterface, kRestartSignal,
                       &PowerManDaemon::HandleRestartSignal);
  AddDBusSignalHandler(kRootPowerManagerInterface, kRequestCleanShutdown,
                       &PowerManDaemon::HandleRequestCleanShutdownSignal);
  AddDBusSignalHandler(kPowerManagerInterface, kPowerStateChanged,
                       &PowerManDaemon::HandlePowerStateChangedSignal);
  AddDBusSignalHandler(login_manager::kSessionManagerInterface,
                       login_manager::kSessionManagerSessionStateChanged,
                       &PowerManDaemon::HandleSessionManagerStateChangedSignal);

  AddDBusMethodHandler(kRootPowerManagerInterface, kExternalBacklightGetMethod,
                       &PowerManDaemon::HandleExternalBacklightGetMethod);
  AddDBusMethodHandler(kRootPowerManagerInterface, kExternalBacklightSetMethod,
                       &PowerManDaemon::HandleExternalBacklightSetMethod);

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

void PowerManDaemon::SendButtonEventSignal(const std::string& button_name,
                                           ButtonState state) {
  if (state == BUTTON_REPEAT)
    return;
  LOG(INFO) << "Sending button event signal: " << button_name << " is "
            << (state == BUTTON_UP ? "up" : "down");

  // This signal is used by both Chrome and powerd.
  // Both must be updated if it is changed.
  const gchar* button_name_param = button_name.c_str();
  dbus_bool_t down_param = (state == BUTTON_DOWN) ? TRUE : FALSE;
  dbus_int64_t timestamp_param = base::TimeTicks::Now().ToInternalValue();
  chromeos::dbus::Proxy proxy(chromeos::dbus::GetSystemBusConnection(),
                              kPowerManagerServicePath,
                              kPowerManagerInterface);
  DBusMessage* signal = dbus_message_new_signal(kPowerManagerServicePath,
                                                kPowerManagerInterface,
                                                kButtonEventSignal);
  CHECK(signal);
  dbus_message_append_args(signal,
                           DBUS_TYPE_STRING, &button_name_param,
                           DBUS_TYPE_BOOLEAN, &down_param,
                           DBUS_TYPE_INT64, &timestamp_param,
                           DBUS_TYPE_INVALID);
  dbus_g_proxy_send(proxy.gproxy(), signal, NULL);
  dbus_message_unref(signal);
}

void PowerManDaemon::Shutdown() {
  util::Launch("shutdown -P now");
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
