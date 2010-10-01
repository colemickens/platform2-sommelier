// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gdk/gdkx.h>

#include "base/string_util.h"
#include "chromeos/dbus/dbus.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/powerman.h"
#include "power_manager/util.h"


namespace power_manager {

PowerManDaemon::PowerManDaemon(bool use_input_for_lid,
                               bool use_input_for_key_power,
                               PowerPrefs* prefs)
  : loop_(NULL),
    use_input_for_lid_(use_input_for_lid),
    use_input_for_key_power_(use_input_for_key_power),
    prefs_(prefs),
    lidstate_(kLidOpened),
    power_button_state_(false),
    retry_suspend_count_(0),
    retry_suspend_event_id_(0) {}

void PowerManDaemon::Init() {
  int lid_state = 0;
  CHECK(prefs_->ReadSetting("retry_suspend_ms", &retry_suspend_ms_));
  CHECK(prefs_->ReadSetting("retry_suspend_attempts",
                            &retry_suspend_attempts_));
  // retrys will occur no more than once a minute
  CHECK(retry_suspend_ms_ >= 60000);
  // only 1-10 retries prior to just shutting down
  CHECK(retry_suspend_attempts_ > 0);
  CHECK(retry_suspend_attempts_ <= 10);
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

gboolean PowerManDaemon::RetrySuspend(void* object) {
  PowerManDaemon* daemon = static_cast<PowerManDaemon*>(object);
  if(daemon->lidstate_ == kLidClosed) {
    daemon->retry_suspend_count_++;
    if (daemon->retry_suspend_count_ > daemon->retry_suspend_attempts_) {
      LOG(ERROR) << "retry suspend attempts failed ... shutting down";
      daemon->Shutdown();
    } else {
      LOG(WARNING) << "retry suspend " << daemon->retry_suspend_count_;
      daemon->Suspend();
    }
    return true;
  } else {
    daemon->retry_suspend_event_id_ = 0;
    LOG(INFO) << "retry suspend cancelled ... lid is open";
    return false;
  }
}

void PowerManDaemon::OnInputEvent(void* object, InputType type, int value) {
  PowerManDaemon* daemon = static_cast<PowerManDaemon*>(object);
  switch(type) {
    case LID: {
      daemon->lidstate_ = daemon->GetLidState(value);
      LOG(INFO) << "PowerM Daemon - lid "
                << (daemon->lidstate_ == kLidClosed?"closed.":"opened.");
      if (daemon->lidstate_ == kLidClosed) {
        util::SendSignalToPowerD(util::kLidClosed);
      } else {
        daemon->retry_suspend_count_ = 0;
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
  } else {
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }
  return DBUS_HANDLER_RESULT_HANDLED;
}

void PowerManDaemon::RegisterDBusMessageHandler() {
  const std::string filter = StringPrintf("type='signal', interface='%s'",
                                          util::kLowerPowerManagerInterface);
  DBusError error;
  dbus_error_init(&error);
  DBusConnection* connection = dbus_g_connection_get_connection(
      chromeos::dbus::GetSystemBusConnection().g_connection());
  dbus_bus_add_match(connection, filter.c_str(), &error);
  if (dbus_error_is_set(&error)) {
    LOG(ERROR) << "Failed to add a filter:" << error.name << ", message="
               << error.message;
    NOTREACHED();
  } else {
    CHECK(dbus_connection_add_filter(connection, &DBusMessageHandler, this,
                                     NULL));
    LOG(INFO) << "DBus monitoring started";
  }
}

void PowerManDaemon::Shutdown() {
  util::Launch("shutdown -P now");
}

void PowerManDaemon::Suspend() {
  util::Launch("powerd_suspend");
  if (!retry_suspend_event_id_) {
    retry_suspend_event_id_ = g_timeout_add_seconds(
        retry_suspend_ms_ / 1000, &(PowerManDaemon::RetrySuspend), this);
  }
}

}

