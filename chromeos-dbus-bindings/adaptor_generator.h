// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_BINDINGS_ADAPTOR_GENERATOR_H_
#define CHROMEOS_DBUS_BINDINGS_ADAPTOR_GENERATOR_H_

#include <string>
#include <vector>

#include <base/macros.h>

#include "chromeos-dbus-bindings/header_generator.h"
#include "chromeos-dbus-bindings/indented_text.h"

namespace base {

class FilePath;

}  // namespace base

namespace chromeos_dbus_bindings {

class IndentedText;
struct Interface;

class AdaptorGenerator : public HeaderGenerator {
 public:
  static bool GenerateAdaptor(const Interface& interface,
                              const base::FilePath& output_file);

 private:
  friend class AdaptorGeneratorTest;

  // Generates the constructor for the adaptor.
  static void AddConstructor(const Interface& interface,
                             const std::string& class_name,
                             IndentedText *text);

  // Generates the method interface class.
  static void AddMethodInterface(const Interface& interface,
                                 IndentedText *text);

  // Generates adaptor methods to send the signals.
  static void AddSendSignalMethods(const Interface& interface,
                                   IndentedText *text);

  // Generates DBusSignal data members for the signals.
  static void AddSignalDataMembers(const Interface& interface,
                                   IndentedText *text);

  // Generates adaptor accessor methods for the properties.
  static void AddPropertyMethods(const Interface& interface,
                                 IndentedText *text);

  // Generate ExportProperty data members for the properties.
  static void AddPropertyDataMembers(const Interface& interface,
                                     IndentedText *text);

  // Return a variable name based on the given property name.
  static std::string GetPropertyVariableName(const std::string& property_name);

  DISALLOW_COPY_AND_ASSIGN(AdaptorGenerator);
};

}  // namespace chromeos_dbus_bindings

#endif  // CHROMEOS_DBUS_BINDINGS_ADAPTOR_GENERATOR_H_
