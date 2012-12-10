 // Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/common/util_dbus.h"

#include <dbus/dbus-glib-lowlevel.h>
#include <google/protobuf/message_lite.h>
#include <unistd.h>

#include <climits>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "chromeos/dbus/dbus.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/common/power_constants.h"

namespace power_manager {
namespace util {

bool IsSessionStarted() {
  std::string state;
  if (GetSessionState(&state, NULL))
    return (state == "started");
  return (access("/var/run/state/logged-in", F_OK) == 0);
}

bool GetSessionState(std::string* state, std::string* user) {
  chromeos::dbus::Proxy proxy(chromeos::dbus::GetSystemBusConnection(),
                              login_manager::kSessionManagerServiceName,
                              login_manager::kSessionManagerServicePath,
                              login_manager::kSessionManagerInterface);

  GError* error = NULL;
  gchar* state_arg = NULL;
  gchar* user_arg = NULL;
  if (dbus_g_proxy_call(proxy.gproxy(),
                        login_manager::kSessionManagerRetrieveSessionState,
                        &error,
                        G_TYPE_INVALID,
                        G_TYPE_STRING,
                        &state_arg,
                        G_TYPE_STRING,
                        &user_arg,
                        G_TYPE_INVALID)) {
    if (state)
      *state = state_arg;
    if (user)
      *user = user_arg;
    g_free(state_arg);
    g_free(user_arg);
    return true;
  }
  LOG(ERROR) << "Unable to retrieve session state from session manager: "
             << error->message;
  g_error_free(error);
  return false;
}

bool ParseProtocolBufferFromDBusMessage(
    DBusMessage* message,
    google::protobuf::MessageLite* protobuf_out) {
  DCHECK(message);
  DCHECK(protobuf_out);

  char* data = NULL;
  int size = 0;
  if (!dbus_message_get_args(message, NULL,
                             DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
                             &data, &size,
                             DBUS_TYPE_INVALID)) {
    return false;
  }
  return protobuf_out->ParseFromArray(data, size);
}

void AppendProtocolBufferToDBusMessage(
    const google::protobuf::MessageLite& protobuf,
    DBusMessage* message_out) {
  DCHECK(message_out);

  std::string serialized_protobuf;
  CHECK(protobuf.SerializeToString(&serialized_protobuf))
      << "Unable to serialize " << protobuf.GetTypeName() << " protocol buffer";
  const uint8* data =
      reinterpret_cast<const uint8*>(serialized_protobuf.data());
  CHECK(serialized_protobuf.size() <= static_cast<size_t>(INT32_MAX))
      << protobuf.GetTypeName() << " protocol buffer is "
      << serialized_protobuf.size() << " bytes";
  // Per D-Bus documentation: "To append an array of fixed-length basic types
  // (except Unix file descriptors), pass in the DBUS_TYPE_ARRAY typecode, the
  // element typecode, the address of the array pointer, and a 32-bit integer
  // giving the number of elements in the array."
  dbus_message_append_args(message_out, DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &data,
                           static_cast<int32_t>(serialized_protobuf.size()),
                           DBUS_TYPE_INVALID);
}

void SendSignalToSessionManager(const char* signal) {
  chromeos::dbus::Proxy proxy(chromeos::dbus::GetSystemBusConnection(),
                              login_manager::kSessionManagerServiceName,
                              login_manager::kSessionManagerServicePath,
                              login_manager::kSessionManagerInterface);
  GError* error = NULL;
  if (!dbus_g_proxy_call(proxy.gproxy(), signal, &error, G_TYPE_INVALID,
                         G_TYPE_INVALID)) {
    LOG(ERROR) << "Error sending signal: " << error->message;
    g_error_free(error);
  }
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
                        const google::protobuf::MessageLite& protobuf,
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
  AppendProtocolBufferToDBusMessage(protobuf, message);
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
  DCHECK(message);
  DBusMessage* reply = dbus_message_new_method_return(message);
  CHECK(reply);
  return reply;
}

DBusMessage* CreateDBusProtocolBufferReply(
    DBusMessage* message,
    const google::protobuf::MessageLite& protobuf) {
  DBusMessage* reply = CreateEmptyDBusReply(message);
  AppendProtocolBufferToDBusMessage(protobuf, reply);
  return reply;
}

DBusMessage* CreateDBusInvalidArgsErrorReply(DBusMessage* message) {
  DCHECK(message);
  DBusMessage* reply = dbus_message_new_error(
      message, DBUS_ERROR_INVALID_ARGS, "Invalid arguments passed to method");
  CHECK(reply);
  return reply;
}

DBusMessage* CreateDBusErrorReply(DBusMessage* message,
                                  const std::string& details) {
  DCHECK(message);
  DBusMessage* reply = dbus_message_new_error(
      message, DBUS_ERROR_FAILED, details.c_str());
  CHECK(reply);
  return reply;
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

void RequestDBusServiceName(const char* name) {
  DBusConnection* connection = dbus_g_connection_get_connection(
      chromeos::dbus::GetSystemBusConnection().g_connection());
  CHECK(connection);
  DBusError error;
  dbus_error_init(&error);
  if (dbus_bus_request_name(connection, name, 0, &error) < 0) {
    LOG(FATAL) << "Failed to register name \"" << name << "\": "
               << (dbus_error_is_set(&error) ? error.message : "Unknown error");
  }
}

bool IsDBusServiceConnected(const std::string& service_name,
                            const std::string& service_path,
                            const std::string& interface,
                            std::string* connection_name_out) {
  // dbus_g_proxy_new_for_name_owner() is documented as returning NULL if the
  // passed-in name has no owner.
  GError* error = NULL;
  DBusGProxy* proxy = dbus_g_proxy_new_for_name_owner(
      chromeos::dbus::GetSystemBusConnection().g_connection(),
      service_name.c_str(), service_path.c_str(), interface.c_str(), &error);
  if (!proxy) {
    g_error_free(error);
    if (connection_name_out)
      connection_name_out->clear();
    return false;
  }

  if (connection_name_out)
    *connection_name_out = dbus_g_proxy_get_bus_name(proxy);
  g_object_unref(proxy);
  return true;
}

}  // namespace util
}  // namespace power_manager
