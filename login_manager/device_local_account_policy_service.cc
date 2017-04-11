// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/device_local_account_policy_service.h"

#include <stdint.h>

#include <utility>

#include <base/files/file_enumerator.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/memory/ptr_util.h>
#include <base/memory/ref_counted.h>
#include <base/strings/string_util.h>
#include <brillo/cryptohome.h>
#include <chromeos/dbus/service_constants.h>

#include "bindings/chrome_device_policy.pb.h"
#include "login_manager/policy_key.h"
#include "login_manager/policy_service.h"
#include "login_manager/policy_store.h"

namespace em = enterprise_management;

namespace login_manager {

const base::FilePath::CharType DeviceLocalAccountPolicyService::kPolicyDir[] =
    FILE_PATH_LITERAL("policy");

const base::FilePath::CharType
    DeviceLocalAccountPolicyService::kPolicyFileName[] =
        FILE_PATH_LITERAL("policy");

DeviceLocalAccountPolicyService::DeviceLocalAccountPolicyService(
    const base::FilePath& device_local_account_dir,
    PolicyKey* owner_key)
    : device_local_account_dir_(device_local_account_dir),
      owner_key_(owner_key) {}

DeviceLocalAccountPolicyService::~DeviceLocalAccountPolicyService() {}

bool DeviceLocalAccountPolicyService::Store(
    const std::string& account_id,
    const uint8_t* policy_data,
    uint32_t policy_data_size,
    const PolicyService::Completion& completion) {
  PolicyService* service = GetPolicyService(account_id);
  if (!service) {
    PolicyService::Error error(dbus_error::kInvalidAccount,
                               "Invalid device-local account");
    completion.Run(error);
    return false;
  }

  return service->Store(policy_data, policy_data_size, completion,
                        PolicyService::KEY_NONE, SignatureCheck::kEnabled);
}

bool DeviceLocalAccountPolicyService::Retrieve(
    const std::string& account_id,
    std::vector<uint8_t>* policy_data) {
  PolicyService* service = GetPolicyService(account_id);
  if (!service)
    return false;

  return service->Retrieve(policy_data);
}

void DeviceLocalAccountPolicyService::UpdateDeviceSettings(
    const em::ChromeDeviceSettingsProto& device_settings) {
  // Update the policy map.
  typedef google::protobuf::RepeatedPtrField<em::DeviceLocalAccountInfoProto>
      DeviceLocalAccountList;
  std::map<std::string, std::unique_ptr<PolicyService>> new_policy_map;
  const DeviceLocalAccountList& list(
      device_settings.device_local_accounts().account());
  for (DeviceLocalAccountList::const_iterator account(list.begin());
       account != list.end();
       ++account) {
    std::string account_key;
    if (account->has_account_id()) {
      account_key = GetAccountKey(account->account_id());
    } else if (!account->has_type() &&
               account->has_deprecated_public_session_id()) {
      account_key = GetAccountKey(account->deprecated_public_session_id());
    }
    if (!account_key.empty()) {
      new_policy_map[account_key] = std::move(policy_map_[account_key]);
    }
  }
  policy_map_.swap(new_policy_map);

  MigrateUppercaseDirs();

  // Purge all existing on-disk accounts that are no longer defined.
  base::FileEnumerator enumerator(
      device_local_account_dir_, false, base::FileEnumerator::DIRECTORIES);
  base::FilePath subdir;
  while (!(subdir = enumerator.Next()).empty()) {
    if (IsValidAccountKey(subdir.BaseName().value()) &&
        policy_map_.find(subdir.BaseName().value()) == policy_map_.end()) {
      LOG(INFO) << "Purging " << subdir.value();
      if (!base::DeleteFile(subdir, true))
        LOG(ERROR) << "Failed to delete " << subdir.value();
    }
  }
}

bool DeviceLocalAccountPolicyService::MigrateUppercaseDirs(void) {
  base::FileEnumerator enumerator(
      device_local_account_dir_, false, base::FileEnumerator::DIRECTORIES);
  base::FilePath subdir;

  while (!(subdir = enumerator.Next()).empty()) {
    std::string upper = subdir.BaseName().value();
    std::string lower = base::ToLowerASCII(upper);
    if (IsValidAccountKey(lower) && lower != upper) {
      base::FilePath subdir_to(subdir.DirName().Append(lower));
      LOG(INFO) << "Migrating " << upper << " to " << lower;
      if (!base::ReplaceFile(subdir, subdir_to, NULL))
        LOG(ERROR) << "Failed to migrate " << subdir.value();
    }
  }

  return true;
}

PolicyService* DeviceLocalAccountPolicyService::GetPolicyService(
    const std::string& account_id) {
  const std::string key = GetAccountKey(account_id);
  auto entry = policy_map_.find(key);
  if (entry == policy_map_.end())
    return NULL;

  // Lazily create and initialize the policy service instance.
  if (!entry->second) {
    const base::FilePath policy_path =
        device_local_account_dir_
        .AppendASCII(key)
        .Append(kPolicyDir)
        .Append(kPolicyFileName);
    if (!base::CreateDirectory(policy_path.DirName())) {
      LOG(ERROR) << "Failed to create directory for " << policy_path.value();
      return NULL;
    }

    auto store = base::MakeUnique<PolicyStore>(policy_path);
    if (!store->LoadOrCreate()) {
      // This is non-fatal, the policy may not have been stored yet.
      LOG(WARNING) << "Failed to load policy for device-local account "
                   << account_id;
    }
    entry->second =
        base::MakeUnique<PolicyService>(std::move(store), owner_key_);
  }

  return entry->second.get();
}

std::string DeviceLocalAccountPolicyService::GetAccountKey(
    const std::string& account_id) {
  return brillo::cryptohome::home::SanitizeUserName(account_id);
}

bool DeviceLocalAccountPolicyService::IsValidAccountKey(
    const std::string& str) {
  return brillo::cryptohome::home::IsSanitizedUserName(str);
}

}  // namespace login_manager
