// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <string>

#include <base/command_line.h>
#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <base/values.h>
#include <dbus/bus.h>
#include <dbus/object_proxy.h>
#include <dbus/message.h>
#include <sysexits.h>
#include <dbus/values_util.h>

#include "buffet/dbus_constants.h"
#include "buffet/data_encoding.h"

using namespace buffet::dbus_constants;

namespace {
static const int default_timeout_ms = 1000;

dbus::ObjectProxy* GetBuffetDBusProxy(dbus::Bus *bus,
                                      const std::string& object_path) {
  return bus->GetObjectProxy(
      buffet::dbus_constants::kServiceName,
      dbus::ObjectPath(object_path));
}

bool CallTestMethod(dbus::ObjectProxy* proxy) {
  dbus::MethodCall method_call(buffet::dbus_constants::kManagerInterface,
                               buffet::dbus_constants::kManagerTestMethod);
  scoped_ptr<dbus::Response> response(
    proxy->CallMethodAndBlock(&method_call, default_timeout_ms));
  if (!response) {
    std::cout << "Failed to receive a response." << std::endl;
    return false;
  }
  std::cout << "Received a response." << std::endl;
  return true;
}

bool CallManagerCheckDeviceRegistered(dbus::ObjectProxy* proxy) {
  dbus::MethodCall method_call(
    buffet::dbus_constants::kManagerInterface,
    buffet::dbus_constants::kManagerCheckDeviceRegistered);

  scoped_ptr<dbus::Response> response(
    proxy->CallMethodAndBlock(&method_call, default_timeout_ms));
  if (!response) {
    std::cout << "Failed to receive a response." << std::endl;
    return false;
  }

  dbus::MessageReader reader(response.get());
  std::string device_id;
  if (!reader.PopString(&device_id)) {
    std::cout << "No device ID in response." << std::endl;
    return false;
  }

  std::cout << "Device ID: "
            << (device_id.empty() ? std::string("<unregistered>") : device_id)
            << std::endl;
  return true;
}

bool CallManagerGetDeviceInfo(dbus::ObjectProxy* proxy) {
  dbus::MethodCall method_call(
    buffet::dbus_constants::kManagerInterface,
    buffet::dbus_constants::kManagerGetDeviceInfo);

  scoped_ptr<dbus::Response> response(
    proxy->CallMethodAndBlock(&method_call, default_timeout_ms));
  if (!response) {
    std::cout << "Failed to receive a response." << std::endl;
    return false;
  }

  dbus::MessageReader reader(response.get());
  std::string device_info;
  if (!reader.PopString(&device_info)) {
    std::cout << "No device info in response." << std::endl;
    return false;
  }

  std::cout << "Device Info: "
    << (device_info.empty() ? std::string("<unregistered>") : device_info)
    << std::endl;
  return true;
}

bool CallManagerStartRegisterDevice(
    dbus::ObjectProxy* proxy,
    const std::map<std::string, std::shared_ptr<base::Value>>& params) {
  dbus::MethodCall method_call(
      buffet::dbus_constants::kManagerInterface,
      buffet::dbus_constants::kManagerStartRegisterDevice);
  dbus::MessageWriter writer(&method_call);
  dbus::MessageWriter dict_writer(nullptr);
  writer.OpenArray("{sv}", &dict_writer);
  for (auto&& pair : params) {
    dbus::MessageWriter dict_entry_writer(nullptr);
    dict_writer.OpenDictEntry(&dict_entry_writer);
    dict_entry_writer.AppendString(pair.first);
    dbus::AppendBasicTypeValueDataAsVariant(&dict_entry_writer,
                                            *pair.second.get());
    dict_writer.CloseContainer(&dict_entry_writer);
  }
  writer.CloseContainer(&dict_writer);

  static const int timeout_ms = 3000;
  scoped_ptr<dbus::Response> response(
    proxy->CallMethodAndBlock(&method_call, timeout_ms));
  if (!response) {
    std::cout << "Failed to receive a response." << std::endl;
    return false;
  }

  dbus::MessageReader reader(response.get());
  std::string info;
  if (!reader.PopString(&info)) {
    std::cout << "No valid response." << std::endl;
    return false;
  }

  std::cout << "Registration started: " << info << std::endl;
  return true;
}

bool CallManagerFinishRegisterDevice(dbus::ObjectProxy* proxy,
                                     const std::string& user_auth_code) {
  dbus::MethodCall method_call(
    buffet::dbus_constants::kManagerInterface,
    buffet::dbus_constants::kManagerFinishRegisterDevice);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(user_auth_code);
  static const int timeout_ms = 10000;
  scoped_ptr<dbus::Response> response(
    proxy->CallMethodAndBlock(&method_call, timeout_ms));
  if (!response) {
    std::cout << "Failed to receive a response." << std::endl;
    return false;
  }

  dbus::MessageReader reader(response.get());
  std::string device_id;
  if (!reader.PopString(&device_id)) {
    std::cout << "No device ID in response." << std::endl;
    return false;
  }

  std::cout << "Device ID is "
            << (device_id.empty() ? std::string("<unregistered>") : device_id)
            << std::endl;
  return true;
}

bool CallManagerUpdateState(dbus::ObjectProxy* proxy,
                            const std::string& json_blob) {
  dbus::MethodCall method_call(
      buffet::dbus_constants::kManagerInterface,
      buffet::dbus_constants::kManagerUpdateStateMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(json_blob);
  scoped_ptr<dbus::Response> response(
    proxy->CallMethodAndBlock(&method_call, default_timeout_ms));
  if (!response) {
    std::cout << "Failed to receive a response." << std::endl;
    return false;
  }
  return true;
}

void usage() {
  std::cerr << "Possible commands:" << std::endl;
  std::cerr << "  " << kManagerTestMethod << std::endl;
  std::cerr << "  " << kManagerCheckDeviceRegistered << std::endl;
  std::cerr << "  " << kManagerGetDeviceInfo << std::endl;
  std::cerr << "  " << kManagerStartRegisterDevice
                    << " param1 = val1&param2 = val2..." << std::endl;
  std::cerr << "  " << kManagerFinishRegisterDevice
                    << " device_id" << std::endl;
  std::cerr << "  " << kManagerUpdateStateMethod << std::endl;
}

} // namespace

int main(int argc, char** argv) {
  CommandLine::Init(argc, argv);
  CommandLine* cl = CommandLine::ForCurrentProcess();

  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus(new dbus::Bus(options));

  CommandLine::StringVector args = cl->GetArgs();
  if (args.empty()) {
    usage();
    return EX_USAGE;
  }

  // Pop the command off of the args list.
  std::string command = args.front();
  args.erase(args.begin());
  bool success = false;
  if (command.compare(kManagerTestMethod) == 0) {
    auto proxy = GetBuffetDBusProxy(
        bus, buffet::dbus_constants::kManagerServicePath);
    success = CallTestMethod(proxy);
  } else if (command.compare(kManagerCheckDeviceRegistered) == 0 ||
             command.compare("cr") == 0) {
    if (!args.empty()) {
      std::cerr << "Invalid number of arguments for "
        << "Manager." << kManagerCheckDeviceRegistered << std::endl;
      usage();
      return EX_USAGE;
    }
    auto proxy = GetBuffetDBusProxy(
      bus, buffet::dbus_constants::kManagerServicePath);
    success = CallManagerCheckDeviceRegistered(proxy);
  } else if (command.compare(kManagerGetDeviceInfo) == 0 ||
             command.compare("di") == 0) {
    if (!args.empty()) {
      std::cerr << "Invalid number of arguments for "
        << "Manager." << kManagerGetDeviceInfo << std::endl;
      usage();
      return EX_USAGE;
    }
    auto proxy = GetBuffetDBusProxy(
      bus, buffet::dbus_constants::kManagerServicePath);
    success = CallManagerGetDeviceInfo(proxy);
  } else if (command.compare(kManagerStartRegisterDevice) == 0 ||
             command.compare("sr") == 0) {
    if (args.size() > 1) {
      std::cerr << "Invalid number of arguments for "
                << "Manager." << kManagerStartRegisterDevice << std::endl;
      usage();
      return EX_USAGE;
    }
    auto proxy = GetBuffetDBusProxy(
        bus, buffet::dbus_constants::kManagerServicePath);
    std::map<std::string, std::shared_ptr<base::Value>> params;

    if (!args.empty()) {
      auto key_values = chromeos::data_encoding::WebParamsDecode(args.front());
      for (auto&& pair : key_values) {
        params.insert(std::make_pair(
          pair.first, std::shared_ptr<base::Value>(
              base::Value::CreateStringValue(pair.second))));
      }
    }

    success = CallManagerStartRegisterDevice(proxy, params);
  } else if (command.compare(kManagerFinishRegisterDevice) == 0 ||
             command.compare("fr") == 0) {
    if (args.size() > 1) {
      std::cerr << "Invalid number of arguments for "
                << "Manager." << kManagerFinishRegisterDevice << std::endl;
      usage();
      return EX_USAGE;
    }
    auto proxy = GetBuffetDBusProxy(
        bus, buffet::dbus_constants::kManagerServicePath);
    success = CallManagerFinishRegisterDevice(
        proxy, !args.empty() ? args.front() : std::string());
  } else if (command.compare(kManagerUpdateStateMethod) == 0 ||
             command.compare("us") == 0) {
    if (args.size() != 1) {
      std::cerr << "Invalid number of arguments for "
                << "Manager." << kManagerUpdateStateMethod << std::endl;
      usage();
      return EX_USAGE;
    }
    auto proxy = GetBuffetDBusProxy(
        bus, buffet::dbus_constants::kManagerServicePath);
    success = CallManagerUpdateState(proxy, args.front());
  } else {
    std::cerr << "Unknown command: " << command << std::endl;
    usage();
    return EX_USAGE;
  }

  if (success) {
    std::cout << "Done." << std::endl;
    return EX_OK;
  }

  std::cerr << "Done, with errors." << std::endl;
  return 1;
}

