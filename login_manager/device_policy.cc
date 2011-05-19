// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/device_policy.h"

#include <string>

#include <base/basictypes.h>
#include <base/file_path.h>
#include <base/logging.h>

#include "login_manager/bindings/chrome_device_policy.pb.h"
#include "login_manager/bindings/device_management_backend.pb.h"
#include "login_manager/owner_key.h"
#include "login_manager/policy_store.h"
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
    : PolicyStore(policy_path) {
}

DevicePolicy::~DevicePolicy() {
}

bool DevicePolicy::StoreOwnerProperties(OwnerKey* key,
                                        const std::string& current_user,
                                        GError** error) {
  const em::PolicyFetchResponse& policy(Get());
  em::PolicyData poldata;
  if (policy.has_policy_data())
    poldata.ParseFromString(policy.policy_data());
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
  if (poldata.has_username() && poldata.username() == current_user &&
      on_list &&
      key->Equals(policy.new_public_key())) {
    return true;  // No changes are needed.
  }
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
  // write it back.
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
  new_policy.CheckTypeAndMergeFrom(policy);
  new_policy.set_policy_data(new_data);
  new_policy.set_policy_data_signature(
      std::string(reinterpret_cast<const char*>(&sig[0]), sig.size()));
  const std::vector<uint8>& key_der = key->public_key_der();
  new_policy.set_new_public_key(
      std::string(reinterpret_cast<const char*>(&key_der[0]), key_der.size()));
  Set(new_policy);
  return true;
}

bool DevicePolicy::CurrentUserIsOwner(const std::string& current_user) {
  const em::PolicyFetchResponse& policy(Get());
  em::PolicyData poldata;
  if (!policy.has_policy_data())
    return false;
  if (poldata.ParseFromString(policy.policy_data())) {
    return (!poldata.has_request_token() &&
            poldata.has_username() &&
            poldata.username() == current_user);
  }
  return false;
}

}  // namespace login_manager
