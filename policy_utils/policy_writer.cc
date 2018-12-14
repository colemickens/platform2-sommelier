// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "policy_utils/policy_writer.h"

#include <base/files/file_util.h>
#include <base/json/json_writer.h>
#include <base/values.h>

namespace {

// Property name and file name for policy DeviceAllowBluetooth.
const char kPolicyDeviceAllowBluetooth[] = "DeviceAllowBluetooth";
const char kPolicyDeviceAllowBluetoothFileName[] =
    "device_allow_bluetooth.json";

// Converts the given policy to a JSON string and writes it to file
// <dir_path>/<file_name>. Returns whether successul.
bool WritePolicyToFile(const base::DictionaryValue& policy,
                       const base::FilePath& dir_path,
                       const char* file_name) {
  if (!file_name) {
    return false;
  }

  if (!CreateDirectory(dir_path))
    return false;

  std::string json_string;
  base::JSONWriter::Write(policy, &json_string);

  base::FilePath file_path = dir_path.Append(file_name);
  return base::WriteFile(file_path, json_string.data(), json_string.length()) ==
         json_string.length();
}

// Deletes the policy file <dir_path>/<file_name> if it exists. Returns whether
// successful.
bool DeletePolicyFile(const base::FilePath& dir_path, const char* file_name) {
  if (!file_name) {
    return false;
  }
  return base::DeleteFile(dir_path.Append(file_name), false);
}

}  // anonymous namespace

namespace policy_utils {

PolicyWriter::PolicyWriter(const std::string& dest_dir_path)
    : dest_dir_path_(dest_dir_path) {}

PolicyWriter::~PolicyWriter() {}

bool PolicyWriter::SetDeviceAllowBluetooth(bool is_allowed) const {
  base::DictionaryValue policy;
  policy.SetBoolean(kPolicyDeviceAllowBluetooth, is_allowed);
  return WritePolicyToFile(policy, dest_dir_path_,
                           kPolicyDeviceAllowBluetoothFileName);
}

bool PolicyWriter::ClearDeviceAllowBluetooth() const {
  return DeletePolicyFile(dest_dir_path_, kPolicyDeviceAllowBluetoothFileName);
}
}  // namespace policy_utils
