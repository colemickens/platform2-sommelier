// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AUTHPOLICY_POLICY_POLICY_ENCODER_HELPER_H_
#define AUTHPOLICY_POLICY_POLICY_ENCODER_HELPER_H_

#include <string>

#include <base/values.h>
#include <dbus/authpolicy/dbus-constants.h>

namespace base {
class FilePath;
}  // namespace base

namespace policy {

class RegistryDict;

namespace helper {

// Checks a PReg file for existence and loads it into |dict|.
bool LoadPRegFile(const base::FilePath& preg_file,
                  RegistryDict* out_dict,
                  authpolicy::ErrorType* out_error);

// Similar to base::Value::GetAsBoolean(), but in addition it converts int
// values of 0 or 1 to bool. Returns true on success and stores the output in
// bool_value.
bool GetAsBoolean(const base::Value* value, bool* bool_value);

// Same as base::Value::GetAsInteger(), no type conversion (yet).
bool GetAsInteger(const base::Value* value, int* int_value);

// Same as base::Value::GetAsString(), no type conversion (yet).
bool GetAsString(const base::Value* value, std::string* string_value);

void PrintConversionError(const base::Value* value, const char* target_type,
                          const char* policy_name,
                          const std::string* index_str = nullptr);

}  // namespace helper
}  // namespace policy

#endif  // AUTHPOLICY_POLICY_POLICY_ENCODER_HELPER_H_
