// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "authpolicy/policy/windows_policy_encoder.h"

#include <string>

#include <base/json/json_writer.h>
#include <base/values.h>
#include <components/policy/core/common/registry_dict.h>

#include "authpolicy/log_colors.h"
#include "authpolicy/policy/policy_encoder_helper.h"
#include "authpolicy/policy/windows_policy_keys.h"
#include "bindings/authpolicy_containers.pb.h"

using WindowsPolicy = authpolicy::protos::WindowsPolicy;

namespace policy {
namespace {

const char* kColorPolicy = authpolicy::kColorPolicy;
const char* kColorReset = authpolicy::kColorReset;

#define MAP_ENUM(field_name, int_value, enum_value) \
  case int_value:                                   \
    policy->set_##field_name(enum_value);           \
    break;

// Encoders for the interesting policy keys.
void EncodeUserPolicyMode(WindowsPolicy* policy, const base::Value* value) {
  int mode;
  if (!GetAsIntegerInRangeAndPrintError(value, 0, 2, kKeyUserPolicyMode, &mode))
    return;

  switch (mode) {
    MAP_ENUM(user_policy_mode, 0, WindowsPolicy::USER_POLICY_MODE_DEFAULT);
    MAP_ENUM(user_policy_mode, 1, WindowsPolicy::USER_POLICY_MODE_MERGE);
    MAP_ENUM(user_policy_mode, 2, WindowsPolicy::USER_POLICY_MODE_REPLACE);
  }
}

}  // namespace

#undef MAP_ENUM

WindowsPolicyEncoder::WindowsPolicyEncoder(const RegistryDict* dict)
    : dict_(dict) {}

void WindowsPolicyEncoder::EncodePolicy(WindowsPolicy* policy) const {
  LOG_IF(INFO, log_policy_values_)
      << kColorPolicy << "Windows policy" << kColorReset;

  policy->Clear();
  EncodeSinglePolicy(policy, kKeyUserPolicyMode, &EncodeUserPolicyMode);
}

void WindowsPolicyEncoder::EncodeSinglePolicy(WindowsPolicy* policy,
                                              const char* policy_name,
                                              PolicyEncoder encoder) const {
  auto iter = dict_->values().find(policy_name);
  if (iter == dict_->values().end())
    return;

  const base::Value* value = iter->second.get();
  if (!value)
    return;

  encoder(policy, value);

  // Note that Value::operator<< appends newlines, which look ugly in logs.
  std::string value_json;
  base::JSONWriter::Write(*value, &value_json);
  LOG_IF(INFO, log_policy_values_) << kColorPolicy << "  " << policy_name
                                   << " = " << value_json << kColorReset;
}

}  // namespace policy
