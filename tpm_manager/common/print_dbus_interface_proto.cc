// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// THIS CODE IS GENERATED.

#include "tpm_manager/common/print_dbus_interface_proto.h"

#include <string>

#include <base/strings/string_number_conversions.h>
#include <base/strings/stringprintf.h>

#include "tpm_manager/common/print_local_data_proto.h"

namespace tpm_manager {

std::string GetProtoDebugString(TpmManagerStatus value) {
  return GetProtoDebugStringWithIndent(value, 0);
}

std::string GetProtoDebugStringWithIndent(TpmManagerStatus value,
                                          int indent_size) {
  if (value == STATUS_SUCCESS) {
    return "STATUS_SUCCESS";
  }
  if (value == STATUS_UNEXPECTED_DEVICE_ERROR) {
    return "STATUS_UNEXPECTED_DEVICE_ERROR";
  }
  if (value == STATUS_NOT_AVAILABLE) {
    return "STATUS_NOT_AVAILABLE";
  }
  return "<unknown>";
}

std::string GetProtoDebugString(const GetTpmStatusRequest& value) {
  return GetProtoDebugStringWithIndent(value, 0);
}

std::string GetProtoDebugStringWithIndent(const GetTpmStatusRequest& value,
                                          int indent_size) {
  std::string indent(indent_size, ' ');
  std::string output =
      base::StringPrintf("[%s] {\n", value.GetTypeName().c_str());

  output += indent + "}\n";
  return output;
}

std::string GetProtoDebugString(const GetTpmStatusReply& value) {
  return GetProtoDebugStringWithIndent(value, 0);
}

std::string GetProtoDebugStringWithIndent(const GetTpmStatusReply& value,
                                          int indent_size) {
  std::string indent(indent_size, ' ');
  std::string output =
      base::StringPrintf("[%s] {\n", value.GetTypeName().c_str());

  if (value.has_status()) {
    output += indent + "  status: ";
    base::StringAppendF(
        &output, "%s",
        GetProtoDebugStringWithIndent(value.status(), indent_size + 2).c_str());
    output += "\n";
  }
  if (value.has_enabled()) {
    output += indent + "  enabled: ";
    base::StringAppendF(&output, "%s", value.enabled() ? "true" : "false");
    output += "\n";
  }
  if (value.has_owned()) {
    output += indent + "  owned: ";
    base::StringAppendF(&output, "%s", value.owned() ? "true" : "false");
    output += "\n";
  }
  if (value.has_local_data()) {
    output += indent + "  local_data: ";
    base::StringAppendF(&output, "%s", GetProtoDebugStringWithIndent(
                                           value.local_data(), indent_size + 2)
                                           .c_str());
    output += "\n";
  }
  if (value.has_dictionary_attack_counter()) {
    output += indent + "  dictionary_attack_counter: ";
    base::StringAppendF(&output, "%d", value.dictionary_attack_counter());
    output += "\n";
  }
  if (value.has_dictionary_attack_threshold()) {
    output += indent + "  dictionary_attack_threshold: ";
    base::StringAppendF(&output, "%d", value.dictionary_attack_threshold());
    output += "\n";
  }
  if (value.has_dictionary_attack_lockout_in_effect()) {
    output += indent + "  dictionary_attack_lockout_in_effect: ";
    base::StringAppendF(
        &output, "%s",
        value.dictionary_attack_lockout_in_effect() ? "true" : "false");
    output += "\n";
  }
  if (value.has_dictionary_attack_lockout_seconds_remaining()) {
    output += indent + "  dictionary_attack_lockout_seconds_remaining: ";
    base::StringAppendF(&output, "%d",
                        value.dictionary_attack_lockout_seconds_remaining());
    output += "\n";
  }
  output += indent + "}\n";
  return output;
}

std::string GetProtoDebugString(const TakeOwnershipRequest& value) {
  return GetProtoDebugStringWithIndent(value, 0);
}

std::string GetProtoDebugStringWithIndent(const TakeOwnershipRequest& value,
                                          int indent_size) {
  std::string indent(indent_size, ' ');
  std::string output =
      base::StringPrintf("[%s] {\n", value.GetTypeName().c_str());

  output += indent + "}\n";
  return output;
}

std::string GetProtoDebugString(const TakeOwnershipReply& value) {
  return GetProtoDebugStringWithIndent(value, 0);
}

std::string GetProtoDebugStringWithIndent(const TakeOwnershipReply& value,
                                          int indent_size) {
  std::string indent(indent_size, ' ');
  std::string output =
      base::StringPrintf("[%s] {\n", value.GetTypeName().c_str());

  if (value.has_status()) {
    output += indent + "  status: ";
    base::StringAppendF(
        &output, "%s",
        GetProtoDebugStringWithIndent(value.status(), indent_size + 2).c_str());
    output += "\n";
  }
  output += indent + "}\n";
  return output;
}

}  // namespace tpm_manager
