// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_BINDINGS_ADAPTOR_GENERATOR_H_
#define CHROMEOS_DBUS_BINDINGS_ADAPTOR_GENERATOR_H_

#include <string>
#include <vector>

#include <base/macros.h>

#include "chromeos-dbus-bindings/indented_text.h"

namespace base {

class FilePath;

}  // namespace base

namespace chromeos_dbus_bindings {

class IndentedText;
struct Interface;

class AdaptorGenerator {
 public:
  AdaptorGenerator() = default;
  virtual ~AdaptorGenerator() = default;

  bool GenerateAdaptor(const Interface& interface,
                       const base::FilePath& output_file);

 private:
  friend class AdaptorGeneratorTest;

  // Create a unique header guard string to protect multiple includes of header.
  static std::string GenerateHeaderGuard(const std::string& filename,
                                         const std::string& interface_name);

  // Generates the constructor for the adaptor.
  static void AddConstructor(const Interface& interface,
                             const std::string& class_name,
                             IndentedText *text);

  // Generates the method interface class.
  static void AddMethodInterface(const Interface& interface,
                                 IndentedText *text);

  // Returns a vector of nesting namepsaces.
  static bool GetNamespacesAndClassName(const std::string& interface_name,
                                        std::vector<std::string>* namespaces,
                                        std::string* class_name);

  // Used to decide whether the argument should be a const reference.
  static bool IsIntegralType(const std::string& type);

  DISALLOW_COPY_AND_ASSIGN(AdaptorGenerator);
};

}  // namespace chromeos_dbus_bindings

#endif  // CHROMEOS_DBUS_BINDINGS_ADAPTOR_GENERATOR_H_
