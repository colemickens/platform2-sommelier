// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_BINDINGS_HEADER_GENERATOR_H_
#define CHROMEOS_DBUS_BINDINGS_HEADER_GENERATOR_H_

#include <string>
#include <vector>

#include <base/macros.h>

namespace base {

class FilePath;

};

namespace chromeos_dbus_bindings {

struct Interface;
class  IndentedText;

class HeaderGenerator {
 protected:
  // Create a unique header guard string to protect multiple includes of header.
  static std::string GenerateHeaderGuard(const base::FilePath& output_file,
                                         const std::string& interface_name);

  // Returns a vector of nesting namepsaces.
  static bool GetNamespacesAndClassName(const std::string& interface_name,
                                        std::vector<std::string>* namespaces,
                                        std::string* class_name);

  // Used to decide whether the argument should be a const reference.
  static bool IsIntegralType(const std::string& type);

  // Writes indented text to a file.
  static bool WriteTextToFile(const base::FilePath& output_file,
                              const IndentedText& text);

  static const int kScopeOffset = 1;
  static const int kBlockOffset = 2;
  static const int kLineContinuationOffset = 4;

 private:
  DISALLOW_COPY_AND_ASSIGN(HeaderGenerator);
};

}  // namespace chromeos_dbus_bindings

#endif  // CHROMEOS_DBUS_BINDINGS_HEADER_GENERATOR_H_
