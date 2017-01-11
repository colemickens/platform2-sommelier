// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "authpolicy/policy/policy_encoder_helper.h"

#include <string>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/strings/string16.h>
#include <base/strings/utf_string_conversions.h>
#include <components/policy/core/common/policy_load_status.h>
#include <components/policy/core/common/preg_parser.h>
#include <components/policy/core/common/registry_dict.h>

namespace {

// Workaround for missing GOOGLE_CHROME_BUILD, always use Chrome OS key for now.
// TODO(ljusten): Find a better fix. See crbug.com/682637.
const base::string16 kRegistryKey =
    base::ASCIIToUTF16("Software\\Policies\\Google\\ChromeOS");

// TODO(ljusten): Copied from latest Chromium base::Value::GetTypeName, remove
// once the latest code is merged.
const char* const kTypeNames[] = {"null",   "boolean", "integer",    "double",
                                  "string", "binary",  "dictionary", "list"};
static_assert(arraysize(kTypeNames) == base::Value::TYPE_LIST + 1,
              "kTypeNames Has Wrong Size");

const char* GetValueTypeName(const base::Value* value) {
  DCHECK_GE(value->GetType(), 0);
  DCHECK_LT(static_cast<size_t>(value->GetType()), arraysize(kTypeNames));
  return kTypeNames[value->GetType()];
}

}  // namespace

namespace policy {
namespace helper {

bool LoadPRegFile(const base::FilePath& preg_file,
                  RegistryDict* out_dict,
                  authpolicy::ErrorType* out_error) {
  if (!base::PathExists(preg_file)) {
    LOG(ERROR) << "PReg file '" << preg_file.value() << "' does not exist";
    *out_error = authpolicy::ERROR_PARSE_PREG_FAILED;
    return false;
  }

  PolicyLoadStatusSample status;
  if (!preg_parser::ReadFile(preg_file, kRegistryKey, out_dict, &status)) {
    LOG(ERROR) << "Failed to parse preg file '" << preg_file.value() << "'";
    *out_error = authpolicy::ERROR_PARSE_PREG_FAILED;
    return false;
  }

  return true;
}

bool GetAsBoolean(const base::Value* value, bool* bool_value) {
  if (value->GetAsBoolean(bool_value))
    return true;

  // Boolean policies are represented as integer 0/1 in the registry.
  int int_value = 0;
  if (value->GetAsInteger(&int_value) && (int_value == 0 || int_value == 1)) {
    *bool_value = int_value != 0;
    return true;
  }

  return false;
}

bool GetAsInteger(const base::Value* value, int* int_value) {
  return value->GetAsInteger(int_value);
}

bool GetAsString(const base::Value* value, std::string* string_value) {
  return value->GetAsString(string_value);
}

// Prints an error log. Used if value cannot be converted to a target type.
void PrintConversionError(const base::Value* value, const char* target_type,
                          const char* policy_name,
                          const std::string* index_str) {
  LOG(ERROR) << "Failed to convert value '" << *value << " of type '"
             << GetValueTypeName(value) << "'"
             << " to " << target_type << " for policy '" << policy_name << "'"
             << (index_str ? " at index " + *index_str : "");
}

}  // namespace helper
}  // namespace policy
