// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "authpolicy/policy/user_policy_encoder.h"

#include <string>
#include <vector>

#include <base/strings/string_number_conversions.h>
#include <base/values.h>
#include <components/policy/core/common/registry_dict.h>

#include "authpolicy/log_colors.h"
#include "authpolicy/policy/policy_encoder_helper.h"
#include "bindings/cloud_policy.pb.h"
#include "bindings/policy_constants.h"

namespace em = enterprise_management;

namespace {

const char* kColorPolicy = authpolicy::kColorPolicy;
const char* kColorReset = authpolicy::kColorReset;

}  // namespace

namespace policy {

UserPolicyEncoder::UserPolicyEncoder(const RegistryDict* dict,
                                     PolicyLevel level)
    : dict_(dict), level_(level) {}

void UserPolicyEncoder::EncodePolicy(em::CloudPolicySettings* policy) const {
  LOG_IF(INFO, log_policy_values_)
      << kColorPolicy << "User policy ("
      << (level_ == POLICY_LEVEL_RECOMMENDED ? "recommended" : "mandatory")
      << ")" << kColorReset;
  EncodeList(policy, kBooleanPolicyAccess, &UserPolicyEncoder::EncodeBoolean);
  EncodeList(policy, kIntegerPolicyAccess, &UserPolicyEncoder::EncodeInteger);
  EncodeList(policy, kStringPolicyAccess, &UserPolicyEncoder::EncodeString);
  EncodeList(policy, kStringListPolicyAccess,
             &UserPolicyEncoder::EncodeStringList);
}

void UserPolicyEncoder::SetPolicyOptions(em::PolicyOptions* options) const {
  DCHECK(options);
  options->set_mode(level_ == POLICY_LEVEL_RECOMMENDED
                        ? em::PolicyOptions_PolicyMode_RECOMMENDED
                        : em::PolicyOptions_PolicyMode_MANDATORY);
}

void UserPolicyEncoder::EncodeBoolean(em::CloudPolicySettings* policy,
                                      const BooleanPolicyAccess* access) const {
  // Try to get policy value from dict.
  const char* policy_name = access->policy_key;
  const base::Value* value = dict_->GetValue(policy_name);
  if (!value)
    return;

  // Get actual value, doing type conversion if necessary.
  bool bool_value;
  if (!GetAsBoolean(value, &bool_value)) {
    PrintConversionError(value, "boolean", policy_name);
    return;
  }

  LOG_IF(INFO, log_policy_values_)
      << kColorPolicy << "  " << policy_name << " = "
      << (bool_value ? "true" : "false") << kColorReset;

  // Create proto and set value.
  em::BooleanPolicyProto* proto = (policy->*access->mutable_proto_ptr)();
  DCHECK(proto);
  proto->set_value(bool_value);
  SetPolicyOptions(proto->mutable_policy_options());
}

void UserPolicyEncoder::EncodeInteger(em::CloudPolicySettings* policy,
                                      const IntegerPolicyAccess* access) const {
  // Try to get policy value from dict.
  const char* policy_name = access->policy_key;
  const base::Value* value = dict_->GetValue(policy_name);
  if (!value)
    return;

  // Get actual value, doing type conversion if necessary.
  int int_value;
  if (!GetAsInteger(value, &int_value)) {
    PrintConversionError(value, "integer", policy_name);
    return;
  }

  LOG_IF(INFO, log_policy_values_) << kColorPolicy << "  " << policy_name
                                   << " = " << int_value << kColorReset;

  // Create proto and set value.
  em::IntegerPolicyProto* proto = (policy->*access->mutable_proto_ptr)();
  DCHECK(proto);
  proto->set_value(int_value);
  SetPolicyOptions(proto->mutable_policy_options());
}

void UserPolicyEncoder::EncodeString(em::CloudPolicySettings* policy,
                                     const StringPolicyAccess* access) const {
  // Try to get policy value from dict.
  const char* policy_name = access->policy_key;
  const base::Value* value = dict_->GetValue(policy_name);
  if (!value)
    return;

  // Get actual value, doing type conversion if necessary.
  std::string string_value;
  if (!GetAsString(value, &string_value)) {
    PrintConversionError(value, "string", policy_name);
    return;
  }

  LOG_IF(INFO, log_policy_values_) << kColorPolicy << "  " << policy_name
                                   << " = " << string_value << kColorReset;

  // Create proto and set value.
  em::StringPolicyProto* proto = (policy->*access->mutable_proto_ptr)();
  DCHECK(proto);
  *proto->mutable_value() = string_value;
  SetPolicyOptions(proto->mutable_policy_options());
}

void UserPolicyEncoder::EncodeStringList(
    em::CloudPolicySettings* policy,
    const StringListPolicyAccess* access) const {
  // Try to get policy key from dict.
  const char* policy_name = access->policy_key;
  const RegistryDict* key = dict_->GetKey(policy_name);
  if (!key)
    return;

  // Get and check all values. Do this in advance to prevent partial writes.
  std::vector<std::string> string_values;
  for (int index = 0; /* empty */; ++index) {
    std::string indexStr = base::IntToString(index + 1);
    const base::Value* value = key->GetValue(indexStr);
    if (!value)
      break;

    std::string string_value;
    if (!GetAsString(value, &string_value)) {
      PrintConversionError(value, "string", policy_name);
      return;
    }

    string_values.push_back(string_value);
  }

  if (log_policy_values_ && LOG_IS_ON(INFO)) {
    LOG(INFO) << kColorPolicy << "  " << policy_name << " = " << kColorReset;
    for (const std::string& value : string_values)
      LOG(INFO) << kColorPolicy << "    " << value << kColorReset;
  }

  // Create proto and set value.
  em::StringListPolicyProto* proto = (policy->*access->mutable_proto_ptr)();
  DCHECK(proto);
  em::StringList* proto_list = proto->mutable_value();
  DCHECK(proto_list);
  proto_list->clear_entries();
  for (const std::string& value : string_values)
    *proto_list->add_entries() = value;
  SetPolicyOptions(proto->mutable_policy_options());
}

template <typename T_Access>
void UserPolicyEncoder::EncodeList(em::CloudPolicySettings* policy,
                                   const T_Access* access,
                                   Encoder<T_Access> encode) const {
  // Access lists are NULL-terminated.
  for (; access->policy_key && access->mutable_proto_ptr; ++access)
    (this->*encode)(policy, access);
}

}  // namespace policy
