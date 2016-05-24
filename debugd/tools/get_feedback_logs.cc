// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is meant for debugging use to manually trigger collection
// of all logs, even the big feedback logs. Normally this can be done with
// dbus-send but dbus-send does not support passing file descriptors.

#include <unistd.h>

#include <memory>

#include <chromeos/dbus/service_constants.h>
#include <dbus/bus.h>
#include <dbus/file_descriptor.h>
#include <dbus/message.h>
#include <dbus/object_proxy.h>

// Because the big logs can be very huge, we set the D-Bus timeout to 2 minutes.
const int kBigLogsDBusTimeoutMS = 120 * 1000;

int main(int argc, char* argv[]) {
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus(new dbus::Bus(options));
  CHECK(bus->Connect());
  dbus::ObjectProxy* debugd_proxy = bus->GetObjectProxy(
      debugd::kDebugdServiceName,
      dbus::ObjectPath(debugd::kDebugdServicePath));

  // Send request for big feedback logs.
  dbus::MethodCall method_call(
      debugd::kDebugdInterface,
      debugd::kGetBigFeedbackLogs);
  dbus::MessageWriter writer(&method_call);
  dbus::FileDescriptor stdout_fd(1);
  stdout_fd.CheckValidity();
  writer.AppendFileDescriptor(stdout_fd);

  std::unique_ptr<dbus::Response> response(
      debugd_proxy->CallMethodAndBlock(
          &method_call, kBigLogsDBusTimeoutMS));
  CHECK(response) << debugd::kGetBigFeedbackLogs << " failed";
}
