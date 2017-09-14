// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AUTHPOLICY_POLICY_POLICY_ENCODER_HELPER_H_
#define AUTHPOLICY_POLICY_POLICY_ENCODER_HELPER_H_

#include <string>

#include <base/values.h>

namespace base {
class FilePath;
}  // namespace base

namespace policy {

class RegistryDict;

// Registry key path for user/device policy.
extern const char kKeyUserDevice[];

// Registry key path for Chrome extension policy.
extern const char kKeyExtensions[];

// Registry key for recommended user and extension policy.
extern const char kKeyRecommended[];

// Registry key for mandatory extension policy. Note that mandatory user
// policy doesn't get any extension.
extern const char kKeyMandatoryExtension[];

// Checks a PReg file for existence and loads all entries in the branch with
// root |registry_key| into |dict|.
bool LoadPRegFile(const base::FilePath& preg_file,
                  const char* registry_key,
                  RegistryDict* dict);

// Similar to base::Value::GetAsBoolean(), but in addition it converts int
// values of 0 or 1 to bool. Returns true on success and stores the output in
// bool_value.
bool GetAsBoolean(const base::Value* value, bool* bool_value);

// Same as base::Value::GetAsInteger(), no type conversion (yet).
bool GetAsInteger(const base::Value* value, int* int_value);

// Same as base::Value::GetAsString(), no type conversion (yet).
bool GetAsString(const base::Value* value, std::string* string_value);

void PrintConversionError(const base::Value* value,
                          const char* target_type,
                          const char* policy_name,
                          const std::string* index_str = nullptr);

}  // namespace policy

#endif  // AUTHPOLICY_POLICY_POLICY_ENCODER_HELPER_H_
