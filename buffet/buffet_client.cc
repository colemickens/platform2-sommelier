// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <string>

#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <dbus/bus.h>
#include <dbus/object_proxy.h>
#include <dbus/message.h>

#include "buffet/dbus_constants.h"
#include "buffet/dbus_manager.h"

using namespace buffet::dbus_constants;

namespace {

dbus::ObjectProxy* GetBuffetDBusProxy(dbus::Bus *bus,
                                      const std::string& object_path) {
  return bus->GetObjectProxy(
      buffet::dbus_constants::kServiceName,
      dbus::ObjectPath(object_path));
}

bool CallTestMethod(dbus::ObjectProxy* proxy) {
  int timeout_ms = 1000;
  dbus::MethodCall method_call(buffet::dbus_constants::kRootInterface,
                               buffet::dbus_constants::kRootTestMethod);
  scoped_ptr<dbus::Response> response(proxy->CallMethodAndBlock(&method_call,
                                                                timeout_ms));
  if (!response) {
    std::cout << "Failed to receive a response." << std::endl;
    return false;
  }
  std::cout << "Received a response." << std::endl;
  return true;
}

bool CallManagerRegisterDevice(dbus::ObjectProxy* proxy,
                               const std::string& client_id,
                               const std::string& client_secret,
                               const std::string& api_key) {
  dbus::MethodCall method_call(
      buffet::dbus_constants::kManagerInterface,
      buffet::dbus_constants::kManagerRegisterDeviceMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(client_id);
  writer.AppendString(client_secret);
  writer.AppendString(api_key);
  int timeout_ms = 1000;
  scoped_ptr<dbus::Response> response(
      proxy->CallMethodAndBlock(&method_call, timeout_ms));
  if (!response) {
    std::cout << "Failed to receive a response." << std::endl;
    return false;
  }

  dbus::MessageReader reader(response.get());
  std::string registration_id;
  if (!reader.PopString(&registration_id)) {
    std::cout << "No registration id in response." << std::endl;
    return false;
  }

  std::cout << "Registration ID is " << registration_id << std::endl;
  return true;
}

bool CallManagerUpdateState(dbus::ObjectProxy* proxy,
                            const std::string& json_blob) {
  dbus::MethodCall method_call(
      buffet::dbus_constants::kManagerInterface,
      buffet::dbus_constants::kManagerUpdateStateMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(json_blob);
  int timeout_ms = 1000;
  scoped_ptr<dbus::Response> response(
      proxy->CallMethodAndBlock(&method_call, timeout_ms));
  if (!response) {
    std::cout << "Failed to receive a response." << std::endl;
    return false;
  }
  return true;
}

void usage() {
  std::cerr << "Possible commands:" << std::endl;
  std::cerr << "  " << kRootTestMethod << std::endl;
  std::cerr << "  " << kManagerRegisterDeviceMethod
            << "  " << " <client id> <client secret> <api key>" << std::endl;
  std::cerr << "  " << kManagerUpdateStateMethod << std::endl;
}

} // namespace

int main(int argc, char** argv) {
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus(new dbus::Bus(options));

  if (argc < 2) {
    usage();
    return -1;
  }

  char* command = argv[1];
  bool success = false;
  if (strcmp(command, kRootTestMethod) == 0) {
    auto proxy = GetBuffetDBusProxy(
        bus, buffet::dbus_constants::kRootServicePath);
    success = CallTestMethod(proxy);
  } else if (strcmp(command, kManagerRegisterDeviceMethod) == 0) {
    if (argc != 5) {
      std::cerr << "Invalid number of arguments for "
                << "Manager.RegisterDevice" << std::endl;
      usage();
      return -1;
    }
    auto proxy = GetBuffetDBusProxy(
        bus, buffet::dbus_constants::kManagerServicePath);
    success = CallManagerRegisterDevice(proxy, argv[2], argv[3], argv[4]);
  } else if (strcmp(command, kManagerUpdateStateMethod) == 0) {
    if (argc != 3) {
      std::cerr << "Invalid number of arguments for "
                << "Manager.UpdateState" << std::endl;
      usage();
      return -1;
    }
    auto proxy = GetBuffetDBusProxy(
        bus, buffet::dbus_constants::kManagerServicePath);
    success = CallManagerUpdateState(proxy, argv[2]);
  } else {
    std::cerr << "Unkown command: " << command << std::endl;
    usage();
    return -1;
  }

  if (success) {
    std::cout << "Done." << std::endl;
    return 0;
  }

  std::cout << "Done, with errors." << std::endl;
  return -1;
}

