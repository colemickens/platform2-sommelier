// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device_policy_service.h"

#include <base/file_path.h>
#include <base/file_util.h>
#include <base/logging.h>
#include <base/message_loop_proxy.h>
#include <base/task.h>

#include "login_manager/bindings/chrome_device_policy.pb.h"
#include "login_manager/bindings/device_management_backend.pb.h"
#include "login_manager/key_generator.h"
#include "login_manager/nss_util.h"
#include "login_manager/owner_key.h"
#include "login_manager/owner_key_loss_mitigator.h"
#include "login_manager/policy_store.h"

namespace em = enterprise_management;

namespace login_manager {
using google::protobuf::RepeatedPtrField;
using std::string;

// static
const char DevicePolicyService::kPolicyPath[] = "/var/lib/whitelist/policy";
// static
const char DevicePolicyService::kSerialRecoveryFlagFile[] =
    "/var/lib/enterprise_serial_number_recovery";
// static
const char DevicePolicyService::kDevicePolicyType[] = "google/chromeos/device";

DevicePolicyService::~DevicePolicyService() {
}

// static
DevicePolicyService* DevicePolicyService::Create(
    OwnerKeyLossMitigator* mitigator,
    const scoped_refptr<base::MessageLoopProxy>& main_loop) {
  NssUtil* nss = NssUtil::Create();
  return new DevicePolicyService(FilePath(kSerialRecoveryFlagFile),
                                 new PolicyStore(FilePath(kPolicyPath)),
                                 new OwnerKey(nss->GetOwnerKeyFilePath()),
                                 main_loop,
                                 nss,
                                 mitigator);
}

bool DevicePolicyService::CheckAndHandleOwnerLogin(
    const std::string& current_user,
    bool* is_owner,
    Error* error) {
  // If the current user is the owner, and isn't whitelisted or set as the owner
  // in the settings blob, then do so.
  bool can_access_key = CurrentUserHasOwnerKey(key()->public_key_der(), error);
  if (can_access_key) {
    if (!StoreOwnerProperties(current_user, error))
      return false;
  }

  // Now, the flip side...if we believe the current user to be the owner based
  // on the user field in policy, and she DOESN'T have the private half of the
  // public key, we must mitigate.
  *is_owner = CurrentUserIsOwner(current_user);
  if (*is_owner && !can_access_key) {
    if (!mitigator_->Mitigate(key()))
      return false;
  }
  return true;
}

bool DevicePolicyService::ValidateAndStoreOwnerKey(
    const std::string& current_user,
    const std::string& buf) {
  std::vector<uint8> pub_key;
  NssUtil::BlobFromBuffer(buf, &pub_key);

  Error error;
  if (!CurrentUserHasOwnerKey(pub_key, &error))
   return false;

  // If we're not mitigating a key loss, we should be able to populate |key_|.
  // If we're mitigating a key loss, we should be able to clobber |key_|.
  if ((!mitigator_->Mitigating() && !key()->PopulateFromBuffer(pub_key)) ||
      (mitigator_->Mitigating() && !key()->ClobberCompromisedKey(pub_key))) {
    return false;
  }
  if (StoreOwnerProperties(current_user, &error)) {
    PersistKey();
    PersistPolicy();
  } else {
    LOG(WARNING) << "Could not immediately store owner properties in policy";
  }
  return true;
}

DevicePolicyService::DevicePolicyService(
    const FilePath& serial_recovery_flag_file,
    PolicyStore* policy_store,
    OwnerKey* policy_key,
    const scoped_refptr<base::MessageLoopProxy>& main_loop,
    NssUtil* nss,
    OwnerKeyLossMitigator* mitigator)
    : PolicyService(policy_store, policy_key, main_loop),
      serial_recovery_flag_file_(serial_recovery_flag_file),
      nss_(nss),
      mitigator_(mitigator) {
}

bool DevicePolicyService::KeyMissing() {
  return key()->HaveCheckedDisk() && !key()->IsPopulated();
}

bool DevicePolicyService::Initialize() {
  bool result = PolicyService::Initialize();
  UpdateSerialNumberRecoveryFlagFile();
  return result;
}

bool DevicePolicyService::Store(const uint8* policy_blob,
                                uint32 len,
                                Completion* completion,
                                int flags) {
  bool result = PolicyService::Store(policy_blob, len, completion, flags);

  if (result)
    UpdateSerialNumberRecoveryFlagFile();

  return result;
}

bool DevicePolicyService::StoreOwnerProperties(const std::string& current_user,
                                               Error* error) {
  const em::PolicyFetchResponse& policy(store()->Get());
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
      key()->Equals(policy.new_public_key())) {
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
  if (!key() || !key()->Sign(data, new_data.length(), &sig)) {
    const char err_msg[] = "Could not sign policy containing new owner data.";
    if (error) {
      LOG(WARNING) << err_msg;
      error->Set(CHROMEOS_LOGIN_ERROR_ILLEGAL_PUBKEY, err_msg);
    } else {
      LOG(ERROR) << err_msg;
    }
    return false;
  }

  em::PolicyFetchResponse new_policy;
  new_policy.CheckTypeAndMergeFrom(policy);
  new_policy.set_policy_data(new_data);
  new_policy.set_policy_data_signature(
      std::string(reinterpret_cast<const char*>(&sig[0]), sig.size()));
  const std::vector<uint8>& key_der = key()->public_key_der();
  new_policy.set_new_public_key(
      std::string(reinterpret_cast<const char*>(&key_der[0]), key_der.size()));
  store()->Set(new_policy);
  return true;
}

bool DevicePolicyService::CurrentUserHasOwnerKey(const std::vector<uint8>& key,
                                                 Error* error) {
  if (!nss_->MightHaveKeys())
    return false;
  if (!nss_->OpenUserDB()) {
    const char msg[] = "Could not open the current user's NSS database.";
    LOG(ERROR) << msg;
    if (error)
      error->Set(CHROMEOS_LOGIN_ERROR_NO_USER_NSSDB, msg);
    return false;
  }
  if (!nss_->GetPrivateKey(key)) {
    const char msg[] = "Could not verify that public key belongs to the owner.";
    LOG(WARNING) << msg;
    if (error)
      error->Set(CHROMEOS_LOGIN_ERROR_ILLEGAL_PUBKEY, msg);
    return false;
  }
  return true;
}

bool DevicePolicyService::CurrentUserIsOwner(const std::string& current_user) {
  const em::PolicyFetchResponse& policy(store()->Get());
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

void DevicePolicyService::UpdateSerialNumberRecoveryFlagFile() {
  const em::PolicyFetchResponse& policy(store()->Get());
  em::PolicyData policy_data;
  if (policy.has_policy_data() &&
      policy_data.ParseFromString(policy.policy_data()) &&
      !policy_data.request_token().empty() &&
      policy_data.valid_serial_number_missing()) {
    if (file_util::WriteFile(serial_recovery_flag_file_, NULL, 0) != 0) {
      PLOG(WARNING) << "Failed to write "
                    << serial_recovery_flag_file_.value();
    }
  } else {
    if (!file_util::Delete(serial_recovery_flag_file_, false)) {
      PLOG(WARNING) << "Failed to delete "
                    << serial_recovery_flag_file_.value();
    }
  }
}

}  // namespace login_manager
