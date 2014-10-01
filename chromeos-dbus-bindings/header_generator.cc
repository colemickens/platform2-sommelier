// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos-dbus-bindings/header_generator.h"

#include <string>

#include <base/file_util.h>
#include <base/files/file_path.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>

#include "chromeos-dbus-bindings/indented_text.h"

using std::string;
using std::vector;

namespace chromeos_dbus_bindings {

// static
string HeaderGenerator::GenerateHeaderGuard(
    const base::FilePath& output_file, const string& interface_name) {
  string guard = base::StringPrintf("____chromeos_dbus_binding___%s__%s",
                                    interface_name.c_str(),
                                    output_file.value().c_str());
  for (auto& c : guard) {
    if (IsAsciiAlpha(c)) {
      c = base::ToUpperASCII(c);
    } else if (!IsAsciiDigit(c)) {
      c = '_';
    }
  }
  return guard;
}

// static
bool HeaderGenerator::GetNamespacesAndClassName(
    const string& interface_name,
    vector<string>* namespaces,
    string* class_name) {
  vector<string> split_namespaces;
  base::SplitString(interface_name, '.', &split_namespaces);
  if (split_namespaces.size() < 2) {
    LOG(ERROR) << "Interface name must have both a domain and object part "
               << "separated by '.'.  Got " << interface_name << " instead.";
    return false;
  }
  *class_name = split_namespaces.back();
  split_namespaces.pop_back();
  namespaces->swap(split_namespaces);
  return true;
}

// static
bool HeaderGenerator::IsIntegralType(const string& type) {
  return type.find("::") == std::string::npos;
}

// static
bool HeaderGenerator::WriteTextToFile(
    const base::FilePath& output_file, const IndentedText &text) {
  string contents = text.GetContents();
  int expected_write_return = contents.size();
  if (base::WriteFile(output_file, contents.c_str(), contents.size()) !=
      expected_write_return) {
    LOG(ERROR) << "Failed to write file " << output_file.value();
    return false;
  }
  return true;
}

}  // namespace chromeos_dbus_bindings
