// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>  // NOLINT(readability/streams)
#include <string>
#include <sysexits.h>

#include <base/command_line.h>
#include <base/logging.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>
#include <base/values.h>
#include <dbus/bus.h>
#include <dbus/message.h>
#include <dbus/object_proxy.h>
#include <dbus/object_manager.h>
#include <dbus/values_util.h>

#include "buffet/data_encoding.h"
#include "buffet/dbus_constants.h"

using namespace buffet::dbus_constants;  // NOLINT(build/namespaces)

namespace {
static const int default_timeout_ms = 1000;

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
  std::cerr << "  " << dbus::kObjectManagerGetManagedObjects << std::endl;
}

class BuffetHelperProxy {
 public:
  int Init() {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    bus_ = new dbus::Bus(options);
    manager_proxy_ = bus_->GetObjectProxy(
        kServiceName,
        dbus::ObjectPath(kManagerServicePath));
    root_proxy_ = bus_->GetObjectProxy(
        kServiceName,
        dbus::ObjectPath(kRootServicePath));
    return EX_OK;
  }

  int CallTestMethod(const CommandLine::StringVector& args) {
    dbus::MethodCall method_call(kManagerInterface, kManagerTestMethod);
    scoped_ptr<dbus::Response> response(
        manager_proxy_->CallMethodAndBlock(&method_call, default_timeout_ms));
    if (!response) {
      std::cout << "Failed to receive a response." << std::endl;
      return EX_UNAVAILABLE;
    }
    std::cout << "Received a response." << std::endl;
    return EX_OK;
  }

  int CallManagerCheckDeviceRegistered(const CommandLine::StringVector& args) {
    if (!args.empty()) {
      std::cerr << "Invalid number of arguments for "
                << "Manager." << kManagerCheckDeviceRegistered << std::endl;
      usage();
      return EX_USAGE;
    }
    dbus::MethodCall method_call(
        kManagerInterface, kManagerCheckDeviceRegistered);

    scoped_ptr<dbus::Response> response(
        manager_proxy_->CallMethodAndBlock(&method_call, default_timeout_ms));
    if (!response) {
      std::cout << "Failed to receive a response." << std::endl;
      return EX_UNAVAILABLE;
    }

    dbus::MessageReader reader(response.get());
    std::string device_id;
    if (!reader.PopString(&device_id)) {
      std::cout << "No device ID in response." << std::endl;
      return EX_SOFTWARE;
    }

    std::cout << "Device ID: "
              << (device_id.empty() ? std::string("<unregistered>") : device_id)
              << std::endl;
    return EX_OK;
  }

  int CallManagerGetDeviceInfo(const CommandLine::StringVector& args) {
    if (!args.empty()) {
      std::cerr << "Invalid number of arguments for "
                << "Manager." << kManagerGetDeviceInfo << std::endl;
      usage();
      return EX_USAGE;
    }
    dbus::MethodCall method_call(
        kManagerInterface, kManagerGetDeviceInfo);

    scoped_ptr<dbus::Response> response(
        manager_proxy_->CallMethodAndBlock(&method_call, default_timeout_ms));
    if (!response) {
      std::cout << "Failed to receive a response." << std::endl;
      return EX_UNAVAILABLE;
    }

    dbus::MessageReader reader(response.get());
    std::string device_info;
    if (!reader.PopString(&device_info)) {
      std::cout << "No device info in response." << std::endl;
      return EX_SOFTWARE;
    }

    std::cout << "Device Info: "
      << (device_info.empty() ? std::string("<unregistered>") : device_info)
      << std::endl;
    return EX_OK;
  }

  int CallManagerStartRegisterDevice(const CommandLine::StringVector& args) {
    if (args.size() > 1) {
      std::cerr << "Invalid number of arguments for "
                << "Manager." << kManagerStartRegisterDevice << std::endl;
      usage();
      return EX_USAGE;
    }
    std::map<std::string, std::shared_ptr<base::Value>> params;

    if (!args.empty()) {
      auto key_values = buffet::data_encoding::WebParamsDecode(args.front());
      for (const auto& pair : key_values) {
        params.insert(std::make_pair(
          pair.first, std::shared_ptr<base::Value>(
              base::Value::CreateStringValue(pair.second))));
      }
    }

    dbus::MethodCall method_call(
        kManagerInterface, kManagerStartRegisterDevice);
    dbus::MessageWriter writer(&method_call);
    dbus::MessageWriter dict_writer(nullptr);
    writer.OpenArray("{sv}", &dict_writer);
    for (const auto& pair : params) {
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
        manager_proxy_->CallMethodAndBlock(&method_call, timeout_ms));
    if (!response) {
      std::cout << "Failed to receive a response." << std::endl;
      return EX_UNAVAILABLE;
    }

    dbus::MessageReader reader(response.get());
    std::string info;
    if (!reader.PopString(&info)) {
      std::cout << "No valid response." << std::endl;
      return EX_SOFTWARE;
    }

    std::cout << "Registration started: " << info << std::endl;
    return EX_OK;
  }

  int CallManagerFinishRegisterDevice(const CommandLine::StringVector& args) {
    if (args.size() > 1) {
      std::cerr << "Invalid number of arguments for "
                << "Manager." << kManagerFinishRegisterDevice << std::endl;
      usage();
      return EX_USAGE;
    }
    dbus::MethodCall method_call(
        kManagerInterface, kManagerFinishRegisterDevice);
    dbus::MessageWriter writer(&method_call);
    std::string user_auth_code;
    if (!args.empty()) { user_auth_code = args.front(); }
    writer.AppendString(user_auth_code);
    static const int timeout_ms = 10000;
    scoped_ptr<dbus::Response> response(
        manager_proxy_->CallMethodAndBlock(&method_call, timeout_ms));
    if (!response) {
      std::cout << "Failed to receive a response." << std::endl;
      return EX_UNAVAILABLE;
    }

    dbus::MessageReader reader(response.get());
    std::string device_id;
    if (!reader.PopString(&device_id)) {
      std::cout << "No device ID in response." << std::endl;
      return EX_SOFTWARE;
    }

    std::cout << "Device ID is "
              << (device_id.empty() ? std::string("<unregistered>") : device_id)
              << std::endl;
    return EX_OK;
  }

  int CallManagerUpdateState(const CommandLine::StringVector& args) {
    if (args.size() != 1) {
      std::cerr << "Invalid number of arguments for "
                << "Manager." << kManagerUpdateStateMethod << std::endl;
      usage();
      return EX_USAGE;
    }
    dbus::MethodCall method_call(
        kManagerInterface, kManagerUpdateStateMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(args.front());
    scoped_ptr<dbus::Response> response(
        manager_proxy_->CallMethodAndBlock(&method_call, default_timeout_ms));
    if (!response) {
      std::cout << "Failed to receive a response." << std::endl;
      return EX_UNAVAILABLE;
    }
    return EX_OK;
  }

  int CallRootGetManagedObjects(const CommandLine::StringVector& args) {
    if (!args.empty()) {
      std::cerr << "Invalid number of arguments for "
                << dbus::kObjectManagerGetManagedObjects << std::endl;
      usage();
      return EX_USAGE;
    }
    dbus::MethodCall method_call(
        dbus::kObjectManagerInterface, dbus::kObjectManagerGetManagedObjects);
    scoped_ptr<dbus::Response> response(
        root_proxy_->CallMethodAndBlock(&method_call, default_timeout_ms));
    if (!response) {
      std::cout << "Failed to receive a response." << std::endl;
      return EX_UNAVAILABLE;
    }
    std::cout << response->ToString() << std::endl;
    return EX_OK;
  }

 private:
  scoped_refptr<dbus::Bus> bus_;
  dbus::ObjectProxy* manager_proxy_{nullptr};
  dbus::ObjectProxy* root_proxy_{nullptr};
};

}  // namespace

int main(int argc, char** argv) {
  CommandLine::Init(argc, argv);
  CommandLine* cl = CommandLine::ForCurrentProcess();
  CommandLine::StringVector args = cl->GetArgs();
  if (args.empty()) {
    usage();
    return EX_USAGE;
  }

  // Pop the command off of the args list.
  std::string command = args.front();
  args.erase(args.begin());
  int err = EX_USAGE;
  BuffetHelperProxy helper;
  err = helper.Init();
  if (err) {
    std::cerr << "Error initializing proxies." << std::endl;
    return err;
  }

  if (command.compare(kManagerTestMethod) == 0) {
    err = helper.CallTestMethod(args);
  } else if (command.compare(kManagerCheckDeviceRegistered) == 0 ||
             command.compare("cr") == 0) {
    err = helper.CallManagerCheckDeviceRegistered(args);
  } else if (command.compare(kManagerGetDeviceInfo) == 0 ||
             command.compare("di") == 0) {
    err = helper.CallManagerGetDeviceInfo(args);
  } else if (command.compare(kManagerStartRegisterDevice) == 0 ||
             command.compare("sr") == 0) {
    err = helper.CallManagerStartRegisterDevice(args);
  } else if (command.compare(kManagerFinishRegisterDevice) == 0 ||
             command.compare("fr") == 0) {
    err = helper.CallManagerFinishRegisterDevice(args);
  } else if (command.compare(kManagerUpdateStateMethod) == 0 ||
             command.compare("us") == 0) {
    err = helper.CallManagerUpdateState(args);
  } else if (command.compare(dbus::kObjectManagerGetManagedObjects) == 0) {
    err = helper.CallRootGetManagedObjects(args);
  } else {
    std::cerr << "Unknown command: " << command << std::endl;
    usage();
  }

  if (err) {
    std::cerr << "Done, with errors." << std::endl;
  } else {
    std::cout << "Done." << std::endl;
  }
  return err;
}
