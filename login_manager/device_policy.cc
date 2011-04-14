// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/device_policy.h"

#include <string>

#include <base/basictypes.h>
#include <base/file_path.h>
#include <base/file_util.h>
#include <base/logging.h>

#include "login_manager/bindings/chrome_device_policy.pb.h"
#include "login_manager/bindings/device_management_backend.pb.h"
#include "login_manager/owner_key.h"
#include "login_manager/system_utils.h"

namespace em = enterprise_management;

namespace login_manager {
using google::protobuf::RepeatedPtrField;
using std::string;

// static
const char DevicePolicy::kDefaultPath[] = "/var/lib/whitelist/policy";
// static
const char DevicePolicy::kDevicePolicyType[] = "google/chromeos/device";

DevicePolicy::DevicePolicy(const FilePath& policy_path)
    : policy_path_(policy_path) {
}

DevicePolicy::~DevicePolicy() {
}

bool DevicePolicy::LoadOrCreate() {
  if (!file_util::PathExists(policy_path_))
    return true;
  std::string polstr;
  if (!file_util::ReadFileToString(policy_path_, &polstr) || polstr.empty()) {
    PLOG(ERROR) << "Could not read policy off disk";
    return false;
  }
  if (!policy_.ParseFromString(polstr)) {
    LOG(ERROR) << "Policy on disk could not be parsed!";
    return false;
  }
  return true;
}

const enterprise_management::PolicyFetchResponse& DevicePolicy::Get() const {
  return policy_;
}

bool DevicePolicy::Persist() {
  SystemUtils utils;
  std::string polstr;
  if (!policy_.SerializeToString(&polstr)) {
    LOG(ERROR) << "Could not be serialize policy!";
    return false;
  }
  return utils.AtomicFileWrite(policy_path_, polstr.c_str(), polstr.length());
}

bool DevicePolicy::SerializeToString(std::string* output) const {
  return policy_.SerializeToString(output);
}

void DevicePolicy::Set(
    const enterprise_management::PolicyFetchResponse& policy) {
  policy_.Clear();
  // This can only fail if |policy| and |policy_| are different types.
  policy_.CheckTypeAndMergeFrom(policy);
}

bool DevicePolicy::StoreOwnerProperties(OwnerKey* key,
                                        const std::string& current_user,
                                        GError** error) {
  em::PolicyData poldata;
  if (policy_.has_policy_data())
    poldata.ParseFromString(policy_.policy_data());
  em::ChromeDeviceSettingsProto polval;
  if (poldata.has_policy_type() &&
      poldata.policy_type() == kDevicePolicyType) {
    if (poldata.has_policy_value())
      polval.ParseFromString(poldata.policy_value());
  } else {
    poldata.set_policy_type(kDevicePolicyType);
  }
  // If there existed some device policy, we've got it now!
  // Update the UserWhitelistProto inside the ChromeDeviceSettingsProto we made.
  em::UserWhitelistProto* whitelist_proto = polval.mutable_user_whitelist();
  bool on_list = false;
  const RepeatedPtrField<string>& whitelist = whitelist_proto->user_whitelist();
  for (RepeatedPtrField<string>::const_iterator it = whitelist.begin();
       it != whitelist.end();
       ++it) {
    if (on_list = (current_user == *it))
      break;
  }
  if (poldata.has_username() && poldata.username() == current_user && on_list)
    return true;  // No changes are needed.

  if (!on_list) {
    // Add owner to the whitelist and turn off whitelist enforcement if it is
    // currently not explicitly turned on or off.
    whitelist_proto->add_user_whitelist(current_user);
    if (!polval.has_allow_new_users())
      polval.mutable_allow_new_users()->set_allow_new_users(true);
  }
  poldata.set_username(current_user);

  // We have now updated the whitelist and owner setting in |polval|.
  // We need to put it into |poldata|, serialize that, sign it, and
  // put both into |policy_|.
  poldata.set_policy_value(polval.SerializeAsString());
  std::string new_data = poldata.SerializeAsString();
  std::vector<uint8> sig;
  const uint8* data = reinterpret_cast<const uint8*>(new_data.c_str());
  if (!key || !key->Sign(data, new_data.length(), &sig)) {
    SystemUtils utils;
    const char err_msg[] = "Could not sign policy containing new owner data.";
    LOG_IF(ERROR, error) << err_msg;
    LOG_IF(WARNING, !error) << err_msg;
    utils.SetGError(error, CHROMEOS_LOGIN_ERROR_ILLEGAL_PUBKEY, err_msg);
    return false;
  }

  em::PolicyFetchResponse new_policy;
  new_policy.CheckTypeAndMergeFrom(policy_);
  new_policy.set_policy_data(new_data);
  new_policy.set_policy_data_signature(
      std::string(reinterpret_cast<const char*>(&sig[0]), sig.size()));
  Set(new_policy);
  return true;
}

bool DevicePolicy::CurrentUserIsOwner(const std::string& current_user) {
  em::PolicyData poldata;
  if (!policy_.has_policy_data())
    return false;
  if (poldata.ParseFromString(policy_.policy_data()))
    return poldata.has_username() && poldata.username() == current_user;
  return false;
}

}  // namespace login_manager
