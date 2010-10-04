// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dbus/dbus-glib-lowlevel.h>

#include "power_manager/suspender.h"

#include "power_manager/util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chromeos/dbus/dbus.h"
#include "chromeos/dbus/service_constants.h"

using base::TimeTicks;
using std::max;
using std::min;

namespace power_manager {

static const unsigned int kScreenLockerTimeoutMS = 3000;
static const unsigned int kMaximumDelayTimeoutMS = 10000;

const char *kError = ".Error";

Suspender::Suspender(ScreenLocker* locker)
    : locker_(locker),
      suspend_delay_timeout_ms_(0),
      suspend_delays_outstanding_(0),
      suspend_requested_(false),
      suspend_sequence_number_(0) {}

void Suspender::Init() {
  RegisterDBusMessageHandler();
}

void Suspender::RequestSuspend() {
  unsigned int timeout_ms;
  suspend_requested_ = true;
  suspend_delays_outstanding_ = suspend_delays_.size();
  // Use current time for sequence number.
  suspend_sequence_number_ = TimeTicks::Now().ToInternalValue() / 1000;
  BroadcastSignalToClients(kSuspendDelay, suspend_sequence_number_);
  // TODO(bleung) : change locker to use the new delayed suspend method
  if (locker_->lock_on_suspend_enabled()) {
    locker_->LockScreen();
    suspend_delays_outstanding_++; // one for the locker. TODO : remove this.
    timeout_ms = max(kScreenLockerTimeoutMS, suspend_delay_timeout_ms_);
  } else {
    timeout_ms = suspend_delay_timeout_ms_;
  }
  timeout_ms = min(kMaximumDelayTimeoutMS, timeout_ms);
  LOG(INFO) << "Request Suspend #" << suspend_sequence_number_
            << " Delay Timeout = " << timeout_ms;
  CheckSuspendTimeoutPayload* payload = new CheckSuspendTimeoutPayload;
  payload->sequence_num = suspend_sequence_number_;
  payload->suspender = this;
  g_timeout_add(timeout_ms, CheckSuspendTimeout, payload);
}

void Suspender::CheckSuspend() {
  if (0 < suspend_delays_outstanding_) {
    suspend_delays_outstanding_--;
    LOG(INFO) << "suspend delays outstanding = " << suspend_delays_outstanding_;
  }
  if (suspend_requested_ && 0 == suspend_delays_outstanding_) {
    suspend_requested_ = false;
    LOG(INFO) << "All suspend delays accounted for. Suspending.";
    Suspend();
  }
}

void Suspender::SuspendReady(DBusMessage* message) {
  std::string client_name = dbus_message_get_sender(message);
  LOG(INFO) << "SuspendReady, client : " << client_name;
  SuspendList::iterator iter = suspend_delays_.find(client_name);
  if (suspend_delays_.end() == iter) {
    LOG(WARNING) << "Unregistered client attempting to ack SuspendReady!";
    return;
  }
  DBusError error;
  dbus_error_init(&error);
  unsigned int sequence_num;
  if (!dbus_message_get_args(message, &error, DBUS_TYPE_UINT32, &sequence_num,
                             DBUS_TYPE_INVALID)) {
    LOG(ERROR) << "Could not get args from SuspendReady signal!";
    if (dbus_error_is_set(&error))
      dbus_error_free(&error);
    return;
  }
  if (sequence_num == suspend_sequence_number_) {
    LOG(INFO) << "Suspend sequence number match! " << sequence_num;
    CheckSuspend();
  } else {
    LOG(INFO) << "Out of sequence SuspendReady ack!";
  }
}

void Suspender::CancelSuspend() {
  if (suspend_requested_)
    LOG(INFO) << "Suspend canceled mid flight.";
  suspend_requested_ = false;
  suspend_delays_outstanding_ = 0;
}

// Register a Suspend Delay client callback. Expected argument is unsigned int32
// containing the expected maximum delay, which will be used for delay timeout.
DBusMessage* Suspender::RegisterSuspendDelay(DBusMessage* message) {
  DBusError error;
  uint32 delay_ms;
  DBusMessage* reply;
  const std::string errmsg = StringPrintf("%s%s", kPowerManagerInterface,
                                          kError);
  dbus_error_init(&error);
  reply = dbus_message_new_method_return(message);
  if (!dbus_message_get_args(message, &error, DBUS_TYPE_UINT32, &delay_ms,
                             DBUS_TYPE_INVALID)) {
    dbus_message_set_error_name(reply, errmsg.c_str());
    LOG(WARNING) << "Failure to register.";
    if (dbus_error_is_set(&error))
      dbus_error_free(&error);
    return reply;
  }
  std::string client_name = dbus_message_get_sender(message);
  LOG(INFO) << "register-suspend-delay, client: " << client_name
            << " delay_ms : " << delay_ms;
  if (0 < delay_ms) {
    suspend_delays_[client_name] = delay_ms;
    if (delay_ms > suspend_delay_timeout_ms_)
     suspend_delay_timeout_ms_ = delay_ms;
  }
  return reply;
}

// Unregister a Suspend Delay client callback.
DBusMessage* Suspender::UnregisterSuspendDelay(DBusMessage* message) {
  DBusMessage* reply;
  const std::string errmsg = StringPrintf("%s%s", kPowerManagerInterface,
                                          kError);
  reply = dbus_message_new_method_return(message);
  std::string client_name = dbus_message_get_sender(message);
  LOG(INFO) << "unregister-suspend-delay, client: " << client_name;
  if (!CleanUpSuspendDelay(client_name)) {
    dbus_message_set_error_name(reply, errmsg.c_str());
  }
  return reply;
}

DBusHandlerResult Suspender::DBusMessageHandler(
    DBusConnection* conn, DBusMessage* message, void* data) {
  Suspender* suspender = static_cast<Suspender*>(data);
  if (dbus_message_is_method_call(message, kPowerManagerInterface,
      kRegisterSuspendDelay)) {
    dbus_uint32_t serial = 0;
    LOG(INFO) << "kRegisterSuspendDelay";
    DBusMessage* reply = suspender->RegisterSuspendDelay(message);
    if (!dbus_connection_send(conn, reply, &serial)) {
      LOG(ERROR) << "dbus_connection_send(): out of memory";
    }
    dbus_message_unref(reply);
  } else if (dbus_message_is_method_call(message, kPowerManagerInterface,
             kUnregisterSuspendDelay)) {
    dbus_uint32_t serial = 0;
    LOG(INFO) << "kUnregisterSuspendDelay";
    DBusMessage* reply = suspender->UnregisterSuspendDelay(message);
    if (!dbus_connection_send(conn, reply, &serial)) {
      LOG(ERROR) << "dbus_connection_send(): out of memory";
    }
    dbus_message_unref(reply);
  } else if (dbus_message_is_signal(message, kPowerManagerInterface,
             kSuspendReady)) {
    suspender->SuspendReady(message);
  } else {
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }
  return DBUS_HANDLER_RESULT_HANDLED;
}

// Register Message Handler with Dbus for Method calls and Signals.
void Suspender::RegisterDBusMessageHandler() {
  const std::string filter = StringPrintf("type='signal', interface='%s'",
                                          kPowerManagerInterface);
  const std::string objmethods = StringPrintf("type='method_call'");
  DBusError error;
  dbus_error_init(&error);
  connection_ = dbus_g_connection_get_connection(
      chromeos::dbus::GetSystemBusConnection().g_connection());
  dbus_bus_add_match(connection_, objmethods.c_str(), &error);
  if (dbus_error_is_set(&error)) {
    LOG(ERROR) << "Failed to add a filter: " << error.name << ": "
               << error.message;
    NOTREACHED();
  }
  dbus_bus_add_match(connection_, filter.c_str(), &error);
  if (dbus_error_is_set(&error)) {
    LOG(ERROR) << "Failed to add a filter:" << error.name << ", message="
               << error.message;
    NOTREACHED();
  } else {
    CHECK(dbus_connection_add_filter(connection_, &DBusMessageHandler, this,
                                     NULL));
    LOG(INFO) << "DBus monitoring started";
  }
  dbus_bus_request_name(connection_, kPowerManagerInterface,
                        DBUS_NAME_FLAG_ALLOW_REPLACEMENT, &error);
  if (dbus_error_is_set(&error)) {
    LOG(ERROR) << "Failed to request name: " << error.name << ": "
               << error.message;
    NOTREACHED();
  }

  DBusGProxy* proxy = dbus_g_proxy_new_for_name(
      chromeos::dbus::GetSystemBusConnection().g_connection(),
      "org.freedesktop.DBus",
      "/org/freedesktop/DBus",
      "org.freedesktop.DBus");
  if (NULL == proxy) {
    LOG(ERROR) << "Failed to connect to freedesktop dbus server.";
    NOTREACHED();
  }
  dbus_g_proxy_add_signal(proxy, "NameOwnerChanged",
                          G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                          G_TYPE_INVALID);
  dbus_g_proxy_connect_signal(proxy, "NameOwnerChanged",
                              G_CALLBACK(NameOwnerChangedHandler), this, NULL);
}

void Suspender::Suspend() {
  util::SendSignalToPowerM(util::kSuspendSignal);
}

gboolean Suspender::CheckSuspendTimeout(gpointer data) {
  CheckSuspendTimeoutPayload* payload;
  payload = static_cast<CheckSuspendTimeoutPayload*>(data);
  Suspender* suspender = payload->suspender;
  if (suspender->suspend_requested_ &&
      suspender->suspend_sequence_number_ == payload->sequence_num) {
    LOG(ERROR) << "Suspend delay timed out. Seq num = "
               << payload->sequence_num;
    suspender->suspend_delays_outstanding_ = 0;
    suspender->CheckSuspend();
  }
  delete(payload);
  // We don't want this callback to be repeated, so we return false.
  return false;
}

void Suspender::NameOwnerChangedHandler(DBusGProxy*,
                                        const gchar* name,
                                        const gchar*,
                                        const gchar* new_owner,
                                        gpointer data) {
  Suspender* suspender = static_cast<Suspender*>(data);
  std::string client_name = name;
  if (0 == strlen(new_owner) && suspender->CleanUpSuspendDelay(client_name))
      LOG(INFO) << name << " deleted for dbus name change.";
}


// Remove |client_name| from list of suspend delay callback clients.
bool Suspender::CleanUpSuspendDelay(std::string client_name) {
  if (suspend_delays_.empty()) {
    return false;
  }
  SuspendList::iterator iter = suspend_delays_.find(client_name);
  if (suspend_delays_.end() == iter) {
    // not a client
    return false;
  }
  LOG(INFO) << "Client " << client_name << " unregistered.";
  unsigned int timeout_ms = iter->second;
  suspend_delays_.erase(iter);
  if (timeout_ms == suspend_delay_timeout_ms_) {
    // find highest timeout value.
    suspend_delay_timeout_ms_ = 0;
    for (iter = suspend_delays_.begin();
         iter != suspend_delays_.end(); ++iter) {
      if (iter->second > suspend_delay_timeout_ms_)
        suspend_delay_timeout_ms_ = iter->second;
    }
  }
  return true;
}

// Broadcast signal, with sequence number payload
void Suspender::BroadcastSignalToClients(const char* signal_name,
                                         const unsigned int sequence_num) {
  dbus_uint32_t payload = sequence_num;
  LOG(INFO) << "Sending Broadcast '" << signal_name << "' to PowerManager:";
  chromeos::dbus::Proxy proxy(chromeos::dbus::GetSystemBusConnection(),
                              "/",
                              power_manager::kPowerManagerInterface);
  DBusMessage* signal = ::dbus_message_new_signal(
      "/",
      power_manager::kPowerManagerInterface,
      signal_name);
  CHECK(signal);
  dbus_message_append_args(signal, DBUS_TYPE_UINT32, &payload);
  ::dbus_g_proxy_send(proxy.gproxy(), signal, NULL);
  ::dbus_message_unref(signal);
}

}  // namespace power_manager
