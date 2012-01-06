// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/external_backlight_client.h"

#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dirent.h>
#include <fcntl.h>
#include <glib.h>
#include <inttypes.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>

#include "base/logging.h"
#include "base/file_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "chromeos/dbus/dbus.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/power_constants.h"

namespace power_manager {

bool ExternalBacklightClient::Init() {
  RegisterDBusMessageHandler();
  return GetActualBrightness(&level_, &max_level_);
}

bool ExternalBacklightClient::GetMaxBrightnessLevel(int64* max_level) {
  CHECK(max_level);
  *max_level = max_level_;
  return true;
}

bool ExternalBacklightClient::GetCurrentBrightnessLevel(int64* current_level ) {
  CHECK(current_level);
  *current_level = level_;
  return true;
}

bool ExternalBacklightClient::SetBrightnessLevel(int64 level) {
  if (level > max_level_ || level < 0) {
    LOG(ERROR) << "SetBrightness level " << level << " is invalid.";
    return false;
  }

  DBusMessage* message = dbus_message_new_method_call(
      kRootPowerManagerServiceName,
      kPowerManagerServicePath,
      kRootPowerManagerInterface,
      kExternalBacklightSetMethod);
  CHECK(message);
  dbus_message_append_args(message,
                           DBUS_TYPE_INT64, &level,
                           DBUS_TYPE_INVALID);
  DBusConnection* connection = dbus_g_connection_get_connection(
      chromeos::dbus::GetSystemBusConnection().g_connection());
  if (dbus_connection_send(connection, message, NULL) == FALSE) {
    LOG(WARNING) << "Error sending " << kExternalBacklightSetMethod
                 << " method call.";
    return false;
  }
  // Save the brightness locally.
  level_ = level;
  return true;
}

bool ExternalBacklightClient::GetActualBrightness(int64* level,
                                                  int64* max_level) {
  CHECK(level);
  CHECK(max_level);
  DBusMessage* message = dbus_message_new_method_call(
      kRootPowerManagerServiceName,
      kPowerManagerServicePath,
      kRootPowerManagerInterface,
      kExternalBacklightGetMethod);
  CHECK(message);
  DBusConnection* connection = dbus_g_connection_get_connection(
      chromeos::dbus::GetSystemBusConnection().g_connection());
  DBusError error;
  dbus_error_init(&error);
  DBusMessage* reply = dbus_connection_send_with_reply_and_block(
      connection, message, -1, &error);
  if (!reply) {
    LOG(WARNING) << "Error sending " << kExternalBacklightGetMethod
                 << " method call: " << error.message;
    dbus_error_free(&error);
    return false;
  }

  dbus_error_init(&error);
  dbus_bool_t result = FALSE;
  if (dbus_message_get_args(reply, &error,
                            DBUS_TYPE_INT64, level,
                            DBUS_TYPE_INT64, max_level,
                            DBUS_TYPE_BOOLEAN, &result,
                            DBUS_TYPE_INVALID) == FALSE) {
    LOG(WARNING) << "Error reading reply from " << kExternalBacklightGetMethod
                 << " method call.";
    return false;
  }
  if (!result)
    return false;
  max_level_ = *max_level;
  return true;
}

DBusHandlerResult ExternalBacklightClient::DBusMessageHandler(
    DBusConnection* /* connection */, DBusMessage* message, void* data) {
  ExternalBacklightClient* client = static_cast<ExternalBacklightClient*>(data);
  if (dbus_message_is_signal(message,
                             kPowerManagerInterface,
                             kExternalBacklightUpdate)) {
    LOG(INFO) << "External backlight changed event";
    DBusError error;
    dbus_error_init(&error);
    if (dbus_message_get_args(message, &error,
                              DBUS_TYPE_INT64, &client->level_,
                              DBUS_TYPE_INT64, &client->max_level_,
                              DBUS_TYPE_INVALID) == FALSE)
      LOG(ERROR) << "Failed to read arguments from signal: " << error.message;
  }
  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

void ExternalBacklightClient::RegisterDBusMessageHandler() {
  DBusConnection* connection = dbus_g_connection_get_connection(
      chromeos::dbus::GetSystemBusConnection().g_connection());
  CHECK(connection);

  DBusError error;
  dbus_error_init(&error);
  std::string match_string =
      StringPrintf("type='signal', interface='%s'", kPowerManagerInterface);
  dbus_bus_add_match(connection, match_string.c_str(), &error);
  if (dbus_error_is_set(&error)) {
    LOG(DFATAL) << "Failed to add match \"" << match_string << "\": "
                << error.name << ", message=" << error.message;
    dbus_error_free(&error);
  }

  CHECK(dbus_connection_add_filter(connection, &DBusMessageHandler, this,
                                   NULL));
  LOG(INFO) << "D-Bus monitoring started.";
}

}  // namespace power_manager
