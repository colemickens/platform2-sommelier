// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <dbus/bus.h>
#include <dbus/object_proxy.h>
#include <gflags/gflags.h>

#include "buffet/dbus_manager.h"

DEFINE_bool(testmethod, false, "Call the Buffet Test Method.");

namespace {

dbus::ObjectProxy* GetBuffetDBusProxy(dbus::Bus *bus) {
  return bus->GetObjectProxy(
      buffet::kBuffetServiceName,
      dbus::ObjectPath(buffet::kBuffetServicePath));
}

void CallTestMethod(dbus::ObjectProxy* proxy) {
  int timeout_ms = 1000;
  dbus::MethodCall method_call(buffet::kBuffetInterface,
                               buffet::kTestMethod);
  scoped_ptr<dbus::Response> response(proxy->CallMethodAndBlock(&method_call,
                                                                timeout_ms));
  if (!response) {
    LOG(ERROR) << "Failed to receive a response.";
    return;
  } else {
    LOG(INFO) << "Received a response.";
  }
}

} // end namespace

int main(int argc, char** argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);

  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus = new dbus::Bus(options);

  auto proxy = GetBuffetDBusProxy(bus);
  if (FLAGS_testmethod) {
    CallTestMethod(proxy);
  }

  LOG(INFO) << "Done.";
  return 0;
}

