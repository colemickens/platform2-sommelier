// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gflags/gflags.h>
#include <stdio.h>

#include "base/at_exit.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "chromeos/dbus/service_constants.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"

DEFINE_bool(set, false, "Set the brightness to --percent");
DEFINE_double(percent, 0, "Percent to set, in the range [0.0, 100.0]");
DEFINE_bool(gradual, true, "Transition gradually");

namespace {

// Queries the brightness from |proxy| and saves it to |percent|, returning true
// on success.
bool GetCurrentBrightness(dbus::ObjectProxy* proxy, double* percent) {
  dbus::MethodCall method_call(
      power_manager::kPowerManagerInterface,
      power_manager::kGetScreenBrightnessPercent);
  scoped_ptr<dbus::Response> response(
      proxy->CallMethodAndBlock(
          &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
  if (!response)
    return false;
  dbus::MessageReader reader(response.get());
  return reader.PopDouble(percent);
}

// Asks |proxy| to set the brightness to |percent| using |style|, returning true
// on success.
bool SetCurrentBrightness(dbus::ObjectProxy* proxy, double percent, int style) {
  dbus::MethodCall method_call(
      power_manager::kPowerManagerInterface,
      power_manager::kSetScreenBrightnessPercent);
  dbus::MessageWriter writer(&method_call);
  writer.AppendDouble(percent);
  writer.AppendInt32(style);
  scoped_ptr<dbus::Response> response(
      proxy->CallMethodAndBlock(
          &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
  return response.get() != NULL;
}

}  // namespace

// A tool to talk to powerd and get or set the backlight level.
int main(int argc, char* argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  CHECK_EQ(argc, 1) << "Unexpected arguments. Try --help";
  base::AtExitManager at_exit_manager;
  base::MessageLoopForIO message_loop;

  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus(new dbus::Bus(options));
  CHECK(bus->Connect());
  dbus::ObjectProxy* powerd_proxy = bus->GetObjectProxy(
      power_manager::kPowerManagerServiceName,
      dbus::ObjectPath(power_manager::kPowerManagerServicePath));

  double percent = 0.0;
  CHECK(GetCurrentBrightness(powerd_proxy, &percent));
  printf("Current percent = %f\n", percent);
  if (FLAGS_set) {
    const int style = FLAGS_gradual ?
        power_manager::kBrightnessTransitionGradual :
        power_manager::kBrightnessTransitionInstant;
    CHECK(SetCurrentBrightness(powerd_proxy, FLAGS_percent, style));
    printf("Set percent to %f\n", FLAGS_percent);
    CHECK(GetCurrentBrightness(powerd_proxy, &percent));
    printf("Current percent now = %f\n", percent);
  }

  return 0;
}
