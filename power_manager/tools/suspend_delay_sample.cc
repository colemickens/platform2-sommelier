// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dbus/dbus-glib-lowlevel.h>
#include <gflags/gflags.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chromeos/dbus/dbus.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/common/dbus_handler.h"
#include "power_manager/common/util_dbus.h"
#include "power_manager/suspend.pb.h"

DEFINE_int32(delay_ms, 5000,
             "Milliseconds to wait before reporting suspend readiness");

DEFINE_int32(timeout_ms, 7000, "Suspend timeout in milliseconds");

namespace {

// Human-readable description of the delay's purpose.
const char kSuspendDelayDescription[] = "suspend_delay_sample";

// ID corresponding to the current suspend attempt.
int suspend_id = 0;

// ID corresponding to the registered suspend delay.
int delay_id = 0;

}  // namespace

power_manager::util::DBusHandler dbus_handler;

// Passes |request| to powerd's |method_name| D-Bus method.
// Copies the returned protocol buffer to |reply_out|, which may be NULL if no
// reply is expected.
bool CallMethod(const std::string& method_name,
                const google::protobuf::MessageLite& request,
                google::protobuf::MessageLite* reply_out) {
  LOG(INFO) << "Calling " << method_name << " method";
  DBusConnection* connection = dbus_g_connection_get_connection(
      chromeos::dbus::GetSystemBusConnection().g_connection());
  CHECK(connection);

  DBusMessage* message = dbus_message_new_method_call(
      power_manager::kPowerManagerServiceName,
      power_manager::kPowerManagerServicePath,
      power_manager::kPowerManagerInterface,
      method_name.c_str());
  CHECK(message);
  power_manager::util::AppendProtocolBufferToDBusMessage(request, message);

  DBusError error;
  dbus_error_init(&error);
  DBusMessage* response = dbus_connection_send_with_reply_and_block(
      connection, message, DBUS_TIMEOUT_USE_DEFAULT, &error);
  dbus_message_unref(message);
  CHECK(!dbus_error_is_set(&error))
      << "Call to " << method_name << " failed: " << error.name
      << " (" << error.message << ")";

  CHECK(!reply_out ||
        power_manager::util::ParseProtocolBufferFromDBusMessage(
            response, reply_out))
      << "Unable to parse response from call to " << method_name;
  dbus_message_unref(response);
  return true;
}

void RegisterSuspendDelay() {
  power_manager::RegisterSuspendDelayRequest request;
  request.set_timeout(
      base::TimeDelta::FromMilliseconds(FLAGS_timeout_ms).ToInternalValue());
  request.set_description(kSuspendDelayDescription);
  power_manager::RegisterSuspendDelayReply reply;
  CHECK(CallMethod(power_manager::kRegisterSuspendDelayMethod,
                   request, &reply));
  delay_id = reply.delay_id();
  LOG(INFO) << "Registered delay with ID " << delay_id;
}

gboolean SendSuspendReady(gpointer) {
  LOG(INFO) << "Calling " << power_manager::kHandleSuspendReadinessMethod;
  power_manager::SuspendReadinessInfo request;
  request.set_delay_id(delay_id);
  request.set_suspend_id(suspend_id);
  CallMethod(power_manager::kHandleSuspendReadinessMethod, request, NULL);
  return FALSE;
}

bool SuspendDelaySignaled(DBusMessage* message) {
  power_manager::SuspendImminent info;
  CHECK(power_manager::util::ParseProtocolBufferFromDBusMessage(message,
                                                                &info));
  suspend_id = info.suspend_id();

  LOG(INFO) << "Got notification about suspend with ID " << suspend_id;
  LOG(INFO) << "Sleeping " << FLAGS_delay_ms << " ms before responding";
  g_timeout_add(FLAGS_delay_ms, SendSuspendReady, NULL);
  return true;
}

void RegisterDBusMessageHandler() {
  dbus_handler.AddSignalHandler(
      power_manager::kPowerManagerInterface,
      power_manager::kSuspendImminentSignal,
      base::Bind(&SuspendDelaySignaled));
  dbus_handler.Start();
}

int main(int argc, char* argv[]) {
  g_type_init();
  google::ParseCommandLineFlags(&argc, &argv, true);
  GMainLoop* loop = g_main_loop_new(NULL, false);
  RegisterDBusMessageHandler();
  RegisterSuspendDelay();
  g_main_loop_run(loop);
  return 0;
}
