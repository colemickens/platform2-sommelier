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
#include "base/time.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/common/power_constants.h"

namespace power_manager {
namespace util {

namespace {

// Maximum amount of time to wait for a reply after making a D-Bus method call.
const int kDBusTimeoutMs = 5000;

// Log a warning if a D-Bus call takes longer than this to complete.
const int kDBusSlowCallMs = 1000;

}  // namespace

DBusGConnection* GetSystemDBusGConnection() {
  GError* error = NULL;
  DBusGConnection* connection = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error);
  CHECK(connection) << "Unable to get system bus connection";
  if (error)
    g_error_free(error);
  return connection;
}

DBusConnection* GetSystemDBusConnection() {
  return dbus_g_connection_get_connection(GetSystemDBusGConnection());
}

DBusMessage* CallDBusMethodAndUnref(DBusMessage* request) {
  const std::string name = dbus_message_get_member(request);
  DBusError error;
  dbus_error_init(&error);
  const base::TimeTicks start_time = base::TimeTicks::Now();
  DBusMessage* reply = dbus_connection_send_with_reply_and_block(
      GetSystemDBusConnection(), request, kDBusTimeoutMs, &error);
  dbus_message_unref(request);

  const base::TimeDelta duration = base::TimeTicks::Now() - start_time;
  if (duration.InMilliseconds() > kDBusSlowCallMs)
    LOG(WARNING) << name << " call took " << duration.InMilliseconds() << " ms";

  if (dbus_error_is_set(&error)) {
    LOG(ERROR) << name << " call failed: " << error.name
               << " (" << error.message << ")";
    dbus_error_free(&error);
  }

  // dbus_connection_send_with_reply_and_block() is documented as returning NULL
  // in the case of an error.
  return reply;
}

bool GetSessionState(std::string* state) {
  DCHECK(state);
  DBusMessage* request = dbus_message_new_method_call(
      login_manager::kSessionManagerServiceName,
      login_manager::kSessionManagerServicePath,
      login_manager::kSessionManagerInterface,
      login_manager::kSessionManagerRetrieveSessionState);
  DBusMessage* reply = CallDBusMethodAndUnref(request);
  if (!reply)
    return false;

  bool success = false;
  DBusError error;
  dbus_error_init(&error);
  const char* state_arg = NULL;
  if (dbus_message_get_args(reply, &error,
                            DBUS_TYPE_STRING, &state_arg,
                            DBUS_TYPE_INVALID)) {
    *state = state_arg;
    success = true;
  } else {
    LOG(ERROR) << "Unable to read "
               << login_manager::kSessionManagerRetrieveSessionState << " args";
  }
  dbus_error_free(&error);
  dbus_message_unref(reply);
  return success;
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

void CallSessionManagerMethod(const std::string& method_name,
                              const char* optional_string_arg) {
  DBusMessage* message = dbus_message_new_method_call(
      login_manager::kSessionManagerServiceName,
      login_manager::kSessionManagerServicePath,
      login_manager::kSessionManagerInterface,
      method_name.c_str());
  CHECK(message);
  if (optional_string_arg) {
    dbus_message_append_args(message, DBUS_TYPE_STRING, &optional_string_arg,
                             DBUS_TYPE_INVALID);
  }

  dbus_connection_send(GetSystemDBusConnection(), message, NULL);
  dbus_message_unref(message);
}

bool CallMethodInPowerD(const char* method_name,
                        const google::protobuf::MessageLite& protobuf,
                        int* return_value) {
  LOG(INFO) << "Calling method '" << method_name << "' in PowerManager";
  DBusMessage* message = dbus_message_new_method_call(kPowerManagerServiceName,
                                                      kPowerManagerServicePath,
                                                      kPowerManagerInterface,
                                                      method_name);
  CHECK(message);
  AppendProtocolBufferToDBusMessage(protobuf, message);
  DBusError error;
  dbus_error_init(&error);
  DBusMessage* response =
      dbus_connection_send_with_reply_and_block(GetSystemDBusConnection(),
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

  if (return_value) {
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
  DBusError error;
  dbus_error_init(&error);
  if (dbus_bus_request_name(GetSystemDBusConnection(), name, 0, &error) < 0) {
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
      GetSystemDBusGConnection(), service_name.c_str(), service_path.c_str(),
      interface.c_str(), &error);
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

std::string GetDBusSender(DBusMessage* message) {
  const char* client_name = dbus_message_get_sender(message);
  if (client_name)
    return std::string(client_name);

  LOG(ERROR) << "dbus_message_get_sender() returned NULL";
  return std::string();
}

}  // namespace util
}  // namespace power_manager
