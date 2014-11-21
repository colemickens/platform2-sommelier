// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>  // NOLINT(readability/streams)
#include <memory>
#include <string>
#include <sysexits.h>

#include <base/command_line.h>
#include <base/logging.h>
#include <base/memory/ref_counted.h>
#include <base/values.h>
#include <chromeos/any.h>
#include <chromeos/data_encoding.h>
#include <chromeos/dbus/data_serialization.h>
#include <chromeos/dbus/dbus_method_invoker.h>
#include <chromeos/errors/error.h>
#include <chromeos/variant_dictionary.h>
#include <dbus/bus.h>
#include <dbus/message.h>
#include <dbus/object_proxy.h>
#include <dbus/object_manager.h>
#include <dbus/values_util.h>

#include "buffet/dbus-proxies.h"
#include "buffet/libbuffet/dbus_constants.h"

using namespace buffet::dbus_constants;  // NOLINT(build/namespaces)

using chromeos::dbus_utils::CallMethodAndBlock;
using chromeos::dbus_utils::CallMethodAndBlockWithTimeout;
using chromeos::dbus_utils::ExtractMethodCallResults;
using chromeos::VariantDictionary;
using chromeos::ErrorPtr;

namespace {

void usage() {
  std::cerr << "Possible commands:" << std::endl;
  std::cerr << "  " << kManagerTestMethod << " <message>" << std::endl;
  std::cerr << "  " << kManagerStartDevice << std::endl;
  std::cerr << "  " << kManagerCheckDeviceRegistered << std::endl;
  std::cerr << "  " << kManagerGetDeviceInfo << std::endl;
  std::cerr << "  " << kManagerRegisterDevice
                    << " param1 = val1&param2 = val2..." << std::endl;
  std::cerr << "  " << kManagerAddCommand
                    << " '{\"name\":\"command_name\",\"parameters\":{}}'"
                    << std::endl;
  std::cerr << "  " << kManagerUpdateStateMethod
                    << " prop_name prop_value" << std::endl;
  std::cerr << "  " << dbus::kObjectManagerGetManagedObjects << std::endl;
}

class BuffetHelperProxy {
 public:
  int Init() {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    bus_ = new dbus::Bus(options);
    manager_proxy_.reset(
      new org::chromium::Buffet::ManagerProxy{bus_, kServiceName,
                                              kManagerServicePath});
    root_proxy_ = bus_->GetObjectProxy(
        kServiceName,
        dbus::ObjectPath(kRootServicePath));
  return EX_OK;
  }

  int CallTestMethod(const CommandLine::StringVector& args) {
    std::string message;
    if (!args.empty())
      message = args.front();

    ErrorPtr error;
    std::string response_message;
    if (!manager_proxy_->TestMethod(message, &response_message, &error)) {
      std::cout << "Failed to receive a response:"
                << error->GetMessage() << std::endl;
      return EX_UNAVAILABLE;
    }

    std::cout << "Received a response: " << response_message << std::endl;
    return EX_OK;
  }

  int CallManagerStartDevice(const CommandLine::StringVector& args) {
    if (!args.empty()) {
      std::cerr << "Invalid number of arguments for "
                << "Manager." << kManagerStartDevice << std::endl;
      usage();
      return EX_USAGE;
    }

    ErrorPtr error;
    if (!manager_proxy_->StartDevice(&error)) {
      std::cout << "Failed to receive a response:"
                << error->GetMessage() << std::endl;
      return EX_UNAVAILABLE;
    }

    return EX_OK;
  }

  int CallManagerCheckDeviceRegistered(const CommandLine::StringVector& args) {
    if (!args.empty()) {
      std::cerr << "Invalid number of arguments for "
                << "Manager." << kManagerCheckDeviceRegistered << std::endl;
      usage();
      return EX_USAGE;
    }

    ErrorPtr error;
    std::string device_id;
    if (!manager_proxy_->CheckDeviceRegistered(&device_id, &error)) {
      std::cout << "Failed to receive a response:"
                << error->GetMessage() << std::endl;
      return EX_UNAVAILABLE;
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

    ErrorPtr error;
    std::string device_info;
    if (!manager_proxy_->GetDeviceInfo(&device_info, &error)) {
      std::cout << "Failed to receive a response:"
                << error->GetMessage() << std::endl;
      return EX_UNAVAILABLE;
    }

    std::cout << "Device Info: "
      << (device_info.empty() ? std::string("<unregistered>") : device_info)
      << std::endl;
    return EX_OK;
  }

  int CallManagerRegisterDevice(const CommandLine::StringVector& args) {
    if (args.size() > 1) {
      std::cerr << "Invalid number of arguments for "
                << "Manager." << kManagerRegisterDevice << std::endl;
      usage();
      return EX_USAGE;
    }

    VariantDictionary params;
    if (!args.empty()) {
      auto key_values = chromeos::data_encoding::WebParamsDecode(args.front());
      for (const auto& pair : key_values) {
        params.insert(std::make_pair(pair.first, chromeos::Any(pair.second)));
      }
    }

    ErrorPtr error;
    std::string device_id;
    if (!manager_proxy_->RegisterDevice(params, &device_id, &error)) {
      std::cout << "Failed to receive a response:"
                << error->GetMessage() << std::endl;
      return EX_UNAVAILABLE;
    }

    std::cout << "Device registered: " << device_id << std::endl;
    return EX_OK;
  }

  int CallManagerUpdateState(const CommandLine::StringVector& args) {
    if (args.size() != 2) {
      std::cerr << "Invalid number of arguments for "
                << "Manager." << kManagerUpdateStateMethod << std::endl;
      usage();
      return EX_USAGE;
    }

    ErrorPtr error;
    VariantDictionary property_set{{args.front(), args.back()}};
    if (!manager_proxy_->UpdateState(property_set, &error)) {
      std::cout << "Failed to receive a response:"
                << error->GetMessage() << std::endl;
      return EX_UNAVAILABLE;
    }
    return EX_OK;
  }

  int CallManagerAddCommand(const CommandLine::StringVector& args) {
    if (args.size() != 1) {
      std::cerr << "Invalid number of arguments for "
                << "Manager." << kManagerAddCommand << std::endl;
      usage();
      return EX_USAGE;
    }

    ErrorPtr error;
    if (!manager_proxy_->AddCommand(args.front(), &error)) {
      std::cout << "Failed to receive a response:"
                << error->GetMessage() << std::endl;
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

    ErrorPtr error;
    auto response = CallMethodAndBlock(
        root_proxy_,
        dbus::kObjectManagerInterface, dbus::kObjectManagerGetManagedObjects,
        &error);
    if (!response) {
      std::cout << "Failed to receive a response:"
                << error->GetMessage() << std::endl;
      return EX_UNAVAILABLE;
    }
    std::cout << response->ToString() << std::endl;
    return EX_OK;
  }

 private:
  scoped_refptr<dbus::Bus> bus_;
  std::unique_ptr<org::chromium::Buffet::ManagerProxy> manager_proxy_;
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
  } else if (command.compare(kManagerStartDevice) == 0 ||
             command.compare("sd") == 0) {
    err = helper.CallManagerStartDevice(args);
  } else if (command.compare(kManagerCheckDeviceRegistered) == 0 ||
             command.compare("cr") == 0) {
    err = helper.CallManagerCheckDeviceRegistered(args);
  } else if (command.compare(kManagerGetDeviceInfo) == 0 ||
             command.compare("di") == 0) {
    err = helper.CallManagerGetDeviceInfo(args);
  } else if (command.compare(kManagerRegisterDevice) == 0 ||
             command.compare("rd") == 0) {
    err = helper.CallManagerRegisterDevice(args);
  } else if (command.compare(kManagerUpdateStateMethod) == 0 ||
             command.compare("us") == 0) {
    err = helper.CallManagerUpdateState(args);
  } else if (command.compare(kManagerAddCommand) == 0 ||
             command.compare("ac") == 0) {
    err = helper.CallManagerAddCommand(args);
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
