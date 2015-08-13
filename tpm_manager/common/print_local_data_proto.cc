// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// THIS CODE IS GENERATED.

#include "tpm_manager/common/print_local_data_proto.h"

#include <string>

#include <base/strings/string_number_conversions.h>
#include <base/strings/stringprintf.h>

namespace tpm_manager {

std::string GetProtoDebugString(const LocalData& value) {
  return GetProtoDebugStringWithIndent(value, 0);
}

std::string GetProtoDebugStringWithIndent(const LocalData& value,
                                          int indent_size) {
  std::string indent(indent_size, ' ');
  std::string output =
      base::StringPrintf("[%s] {\n", value.GetTypeName().c_str());

  if (value.has_owned_by_this_install()) {
    output += indent + "  owned_by_this_install: ";
    base::StringAppendF(&output, "%s",
                        value.owned_by_this_install() ? "true" : "false");
    output += "\n";
  }
  if (value.has_owner_password()) {
    output += indent + "  owner_password: ";
    base::StringAppendF(&output, "%s",
                        base::HexEncode(value.owner_password().data(),
                                        value.owner_password().size())
                            .c_str());
    output += "\n";
  }
  output += indent + "  owner_dependency: {";
  for (int i = 0; i < value.owner_dependency_size(); ++i) {
    base::StringAppendF(&output, "%s", value.owner_dependency(i).c_str());
  }
  output += "}\n";
  output += indent + "}\n";
  return output;
}

}  // namespace tpm_manager
