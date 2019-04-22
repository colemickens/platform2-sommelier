//
// Copyright (C) 2019 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

// THIS CODE IS GENERATED.
// Generated with command:
// ./proto_print.py --subdir common --proto-include attestation/proto_bindings
// ../../system_api/dbus/attestation/keystore.proto

#include "attestation/common/print_keystore_proto.h"

#include <inttypes.h>

#include <string>

#include <base/strings/string_number_conversions.h>
#include <base/strings/stringprintf.h>

namespace attestation {

std::string GetProtoDebugString(KeyType value) {
  return GetProtoDebugStringWithIndent(value, 0);
}

std::string GetProtoDebugStringWithIndent(KeyType value, int indent_size) {
  if (value == KEY_TYPE_RSA) {
    return "KEY_TYPE_RSA";
  }
  if (value == KEY_TYPE_ECC) {
    return "KEY_TYPE_ECC";
  }
  return "<unknown>";
}

std::string GetProtoDebugString(KeyUsage value) {
  return GetProtoDebugStringWithIndent(value, 0);
}

std::string GetProtoDebugStringWithIndent(KeyUsage value, int indent_size) {
  if (value == KEY_USAGE_SIGN) {
    return "KEY_USAGE_SIGN";
  }
  if (value == KEY_USAGE_DECRYPT) {
    return "KEY_USAGE_DECRYPT";
  }
  return "<unknown>";
}

}  // namespace attestation
