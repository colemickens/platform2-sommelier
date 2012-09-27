 // Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/util_dbus.h"

#include <unistd.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "chromeos/dbus/dbus.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/power_constants.h"

namespace power_manager {
namespace util {

bool IsSessionStarted() {
  DBusGConnection* connection =
      chromeos::dbus::GetSystemBusConnection().g_connection();
  CHECK(connection);

  DBusGProxy* proxy = dbus_g_proxy_new_for_name(
      connection,
      login_manager::kSessionManagerServiceName,
      login_manager::kSessionManagerServicePath,
      login_manager::kSessionManagerInterface);

  GError* error = NULL;
  gchar* state = NULL;
  gchar* user = NULL;
  bool result;
  if (dbus_g_proxy_call(proxy,
                        login_manager::kSessionManagerRetrieveSessionState,
                        &error,
                        G_TYPE_INVALID,
                        G_TYPE_STRING,
                        &state,
                        G_TYPE_STRING,
                        &user,
                        G_TYPE_INVALID)) {
    result = (strcmp(state, "started") == 0);
    g_free(state);
    g_free(user);
  } else {
    LOG(ERROR) << "Unable to retrieve session state from session manager: "
               << error->message;
    g_error_free(error);
    result = access("/var/run/state/logged-in", F_OK) == 0;
  }
  g_object_unref(proxy);
  return result;
}

void SendSignalToSessionManager(const char* signal) {
  DBusGConnection* connection;
  GError* error = NULL;
  connection = chromeos::dbus::GetSystemBusConnection().g_connection();
  CHECK(connection);
  DBusGProxy* proxy = dbus_g_proxy_new_for_name(
      connection,
      login_manager::kSessionManagerServiceName,
      login_manager::kSessionManagerServicePath,
      login_manager::kSessionManagerInterface);
  CHECK(proxy);
  error = NULL;
  if (!dbus_g_proxy_call(proxy, signal, &error, G_TYPE_INVALID,
                         G_TYPE_INVALID)) {
    LOG(ERROR) << "Error sending signal: " << error->message;
    g_error_free(error);
  }
  g_object_unref(proxy);
}

void SendSignalToPowerM(const char* signal_name) {
  chromeos::dbus::Proxy proxy(chromeos::dbus::GetSystemBusConnection(),
                              kPowerManagerServicePath,
                              kRootPowerManagerInterface);
  DBusMessage* signal = dbus_message_new_signal(kPowerManagerServicePath,
                                                kRootPowerManagerInterface,
                                                signal_name);
  CHECK(signal);
  dbus_g_proxy_send(proxy.gproxy(), signal, NULL);
  dbus_message_unref(signal);
}

void SendSignalWithUintToPowerM(const char* signal_name, uint32 value) {
  chromeos::dbus::Proxy proxy(chromeos::dbus::GetSystemBusConnection(),
                              kPowerManagerServicePath,
                              kRootPowerManagerInterface);
  DBusMessage* signal = dbus_message_new_signal(kPowerManagerServicePath,
                                                kRootPowerManagerInterface,
                                                signal_name);
  CHECK(signal);
  dbus_message_append_args(signal, DBUS_TYPE_UINT32, &value, DBUS_TYPE_INVALID);
  dbus_g_proxy_send(proxy.gproxy(), signal, NULL);
  dbus_message_unref(signal);
}

void SendSignalWithStringToPowerM(const char* signal_name, const char* string) {
  chromeos::dbus::Proxy proxy(chromeos::dbus::GetSystemBusConnection(),
                              kPowerManagerServicePath,
                              kRootPowerManagerInterface);
  DBusMessage* signal = dbus_message_new_signal(kPowerManagerServicePath,
                                                kRootPowerManagerInterface,
                                                signal_name);
  CHECK(signal);
  dbus_message_append_args(signal, DBUS_TYPE_STRING, &string,
                           DBUS_TYPE_INVALID);
  dbus_g_proxy_send(proxy.gproxy(), signal, NULL);
  dbus_message_unref(signal);
}

void SendSignalToPowerD(const char* signal_name) {
  LOG(INFO) << "Sending signal '" << signal_name << "' to PowerManager";
  chromeos::dbus::Proxy proxy(chromeos::dbus::GetSystemBusConnection(),
                              kPowerManagerServicePath,
                              kPowerManagerInterface);
  DBusMessage* signal = dbus_message_new_signal(kPowerManagerServicePath,
                                                kPowerManagerInterface,
                                                signal_name);
  CHECK(signal);
  dbus_g_proxy_send(proxy.gproxy(), signal, NULL);
  dbus_message_unref(signal);
}

void SendSignalWithIntToPowerD(const char* signal_name, int value) {
  LOG(INFO) << "Sending signal '" << signal_name << "' to PowerManager";
  chromeos::dbus::Proxy proxy(chromeos::dbus::GetSystemBusConnection(),
                              kPowerManagerServicePath,
                              kPowerManagerInterface);
  DBusMessage* signal = dbus_message_new_signal(kPowerManagerServicePath,
                                                kPowerManagerInterface,
                                                signal_name);
  CHECK(signal);
  dbus_message_append_args(signal, DBUS_TYPE_INT32, &value, DBUS_TYPE_INVALID);
  dbus_g_proxy_send(proxy.gproxy(), signal, NULL);
  dbus_message_unref(signal);
}

bool CallMethodInPowerD(const char* method_name,
                        const char* data,
                        uint32 size,
                        int* return_value) {
  LOG(INFO) << "Calling method '" << method_name << "' in PowerManager";
  DBusConnection* connection = dbus_g_connection_get_connection(
      chromeos::dbus::GetSystemBusConnection().g_connection());
  CHECK(connection);
  DBusMessage* message = dbus_message_new_method_call(kPowerManagerServiceName,
                                                      kPowerManagerServicePath,
                                                      kPowerManagerInterface,
                                                      method_name);
  CHECK(message);
  dbus_message_append_args(message,
                           DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
                           &data, size,
                           DBUS_TYPE_INVALID);
  DBusError error;
  dbus_error_init(&error);
  DBusMessage* response =
      dbus_connection_send_with_reply_and_block(connection,
                                                message,
                                                DBUS_TIMEOUT_USE_DEFAULT,
                                                &error);
  dbus_message_unref(message);
  if (dbus_error_is_set(&error)) {
    LOG(ERROR) << "SendMethodToPowerD: method '" << method_name << ": "
               << error.name << " (" << error.message << ")";
    dbus_error_free(&error);
    return false;
  }
  DBusError response_error;
  dbus_error_init(&response_error);
  if (!dbus_message_get_args(response, &response_error,
                             DBUS_TYPE_INT32, return_value,
                             DBUS_TYPE_INVALID)) {
    LOG(WARNING) << "Couldn't read args for '" << method_name
                 << "' response: " << response_error.name
                 << " (" << response_error.message << ")";
    dbus_error_free(&response_error);
    dbus_message_unref(response);
    return false;
  }
  dbus_message_unref(response);
  return true;
}

DBusMessage* CreateEmptyDBusReply(DBusMessage* message) {
  return dbus_message_new_method_return(message);
}

DBusMessage* CreateDBusErrorReply(DBusMessage* message,
                                  const char* error_name,
                                  const char* error_message) {
  return dbus_message_new_error(message, error_name, error_message);
}

void LogDBusError(DBusMessage* message) {
  if (dbus_message_get_type(message) != DBUS_MESSAGE_TYPE_ERROR) {
    LOG(ERROR) << "Received message of non-error type in LogDBusError";
    return;
  }

  char* error_string = NULL;
  if (!dbus_message_get_args(message, NULL,
                             DBUS_TYPE_STRING, &error_string,
                             DBUS_TYPE_INVALID)) {
    LOG(ERROR) << "Could not get arg from dbus error message";
    return;
  }

  if (!error_string) {
    LOG(ERROR) << "Error string for dbus error message is NULL";
    return;
  }

  LOG(INFO) << "Received error message from "
            << dbus_message_get_sender(message) << " with name "
            << dbus_message_get_error_name(message) << ": "
            << error_string;
}

}  // namespace util
}  // namespace power_manager
