// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_BINDINGS_PROXY_GENERATOR_H_
#define CHROMEOS_DBUS_BINDINGS_PROXY_GENERATOR_H_

#include <string>
#include <vector>

#include <base/macros.h>

#include "chromeos-dbus-bindings/header_generator.h"
#include "chromeos-dbus-bindings/indented_text.h"
#include "chromeos-dbus-bindings/interface.h"

namespace base {

class FilePath;

}  // namespace base

namespace chromeos_dbus_bindings {

class IndentedText;
struct Interface;

class ProxyGenerator : public HeaderGenerator {
 public:
  static bool GenerateProxies(const ServiceConfig& config,
                              const std::vector<Interface>& interfaces,
                              const base::FilePath& output_file);

 private:
  friend class ProxyGeneratorTest;

  // Generates one interface proxy.
  static void GenerateInterfaceProxy(const ServiceConfig& config,
                                     const Interface& interface,
                                     IndentedText* text);

  // Generates the constructor and destructor for the proxy.
  static void AddConstructor(const ServiceConfig& config,
                             const Interface& interface,
                             const std::string& class_name,
                             IndentedText* text);
  static void AddDestructor(const std::string& class_name,
                            IndentedText* text);

  // Generates a callback for signal receiver registration completion.
  static void AddSignalConnectedCallback(IndentedText* text);

  // Generates ReleaseObjectProxy() method to release ownership
  // of the object proxy.
  static void AddReleaseObjectProxy(IndentedText* text);

  // Generates the method signatures for signal receivers.
  static void AddSignalReceiver(const Interface& interface,
                                IndentedText* text);

  // Generates a native C++ method which calls a D-Bus method on the proxy.
  static void AddMethodProxy(const Interface::Method& interface,
                             const std::string& interface_name,
                             IndentedText* text);

  // Generates the signal handler name for a given signal name.
  static std::string GetHandlerNameForSignal(const std::string& signal);

  DISALLOW_COPY_AND_ASSIGN(ProxyGenerator);
};

}  // namespace chromeos_dbus_bindings

#endif  // CHROMEOS_DBUS_BINDINGS_PROXY_GENERATOR_H_
