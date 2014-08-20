// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos-dbus-bindings/method_name_generator.h"

#include <string>

#include <base/file_util.h>
#include <base/files/file_path.h>
#include <base/logging.h>

#include "chromeos-dbus-bindings/interface.h"

using std::string;

namespace chromeos_dbus_bindings {

// static
const char MethodNameGenerator::kLineTerminator[] = "\";\n";
const char MethodNameGenerator::kNamePrefix[] = "const char k";
const char MethodNameGenerator::kNameSeparator[] = "Method[] = \"";

bool MethodNameGenerator::GenerateMethodNames(
    const Interface& interface,
    const base::FilePath& output_file) {
  string contents;
  for (const auto& method : interface.methods) {
    const string& method_name = method.name;
    contents.append(
        kNamePrefix + method_name + kNameSeparator + method_name +
        kLineTerminator);
  }

  int expected_write_return = contents.size();
  if (base::WriteFile(output_file, contents.c_str(), contents.size()) !=
      expected_write_return) {
    LOG(ERROR) << "Failed to write file " << output_file.value();
    return false;
  }
  return true;
}

}  // namespace chromeos_dbus_bindings
