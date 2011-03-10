// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/device_policy.h"

#include <string>

#include <base/basictypes.h>
#include <base/file_path.h>
#include <base/file_util.h>
#include <base/logging.h>
#include <base/values.h>
#include <base/json/json_reader.h>
#include <base/json/json_writer.h>
#include <base/scoped_ptr.h>

#include "login_manager/system_utils.h"

namespace login_manager {
// static
const char DevicePolicy::kDefaultPath[] = "/var/lib/whitelist/policy";

// static
const char DevicePolicy::kPolicyField[] = "policy";

// static
const char DevicePolicy::kPolicySigField[] = "signature";

DevicePolicy::DevicePolicy(const FilePath& policy_path)
    : policy_path_(policy_path) {
}

DevicePolicy::~DevicePolicy() {
}

bool DevicePolicy::Load() {
  // This will get blown away if we get a valid dictionary.
  policy_.reset(new DictionaryValue);

  if (!file_util::PathExists(policy_path_))
    return true;
  std::string json;
  if (!file_util::ReadFileToString(policy_path_, &json)) {
    PLOG(ERROR) << "Could not read policy off disk: ";
    return false;
  }

  int error_code = 0;
  std::string error;
  // I take ownership of |new_dict|.
  scoped_ptr<Value> new_val(base::JSONReader::ReadAndReturnError(json,
                                                                 false,
                                                                 &error_code,
                                                                 &error));
  if (!new_val.get()) {
    LOG(ERROR) << "Could not parse prefs off disk: " << error;
    return false;
  }
  if (!new_val->IsType(Value::TYPE_DICTIONARY)) {
    LOG(ERROR) << "Prefs file wasn't a dictionary!";
    return false;
  }
  policy_.reset(static_cast<DictionaryValue*>(new_val.release()));
  return true;
}

bool DevicePolicy::Get(std::string* OUT_policy, std::string* OUT_sig) {
  std::string tmp_policy;
  if (policy_->GetString(kPolicyField, &tmp_policy)) {
    if (!policy_->GetString(kPolicySigField, OUT_sig)) {
      LOG(ERROR) << "Could not find policy signature.";
      return false;
    }
    OUT_policy->assign(tmp_policy);
    return true;
  }
  LOG(ERROR) << "Could not find policy.";
  return false;
}

bool DevicePolicy::Persist() {
  std::string json;
  base::JSONWriter::Write(policy_.get(),
                          true /* pretty print, for now */,
                          &json);
  SystemUtils utils;
  return utils.AtomicFileWrite(policy_path_, json.c_str(), json.length());
}

void DevicePolicy::Set(const std::string& policy, const std::string& sig) {
  policy_->SetString(kPolicyField, policy);
  policy_->SetString(kPolicySigField, sig);
}

}  // namespace login_manager
