// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/device_local_account_policy_service.h"

#include <base/file_util.h>
#include <base/logging.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>
#include <base/message_loop_proxy.h>
#include <base/stl_util.h>
#include <base/string_util.h>
#include <chromeos/cryptohome.h>

#include "login_manager/chrome_device_policy.pb.h"
#include "login_manager/policy_key.h"
#include "login_manager/policy_service.h"
#include "login_manager/policy_store.h"

namespace em = enterprise_management;

namespace login_manager {

const FilePath::CharType DeviceLocalAccountPolicyService::kPolicyDir[] =
    FILE_PATH_LITERAL("policy");
const FilePath::CharType DeviceLocalAccountPolicyService::kPolicyFileName[] =
    FILE_PATH_LITERAL("policy");

DeviceLocalAccountPolicyService::DeviceLocalAccountPolicyService(
    const FilePath& device_local_account_dir,
    PolicyKey* owner_key,
    const scoped_refptr<base::MessageLoopProxy>& main_loop)
    : device_local_account_dir_(device_local_account_dir),
      owner_key_(owner_key),
      main_loop_(main_loop) {
}

DeviceLocalAccountPolicyService::~DeviceLocalAccountPolicyService() {
  policy_map_.clear();
}

bool DeviceLocalAccountPolicyService::Store(const std::string& account_id,
                                      const uint8* policy_data,
                                      uint32 policy_data_size,
                                      PolicyService::Completion* completion) {
  PolicyService* service = GetPolicyService(account_id);
  if (!service) {
    completion->Failure(PolicyService::Error(CHROMEOS_LOGIN_ERROR_INVALID_EMAIL,
                                             "Invalid device-local account"));
    return false;
  }

  // NB: Passing 0 for flags disallows all key changes.
  return service->Store(policy_data, policy_data_size, completion, 0);
}

bool DeviceLocalAccountPolicyService::Retrieve(
    const std::string& account_id,
    std::vector<uint8>* policy_data) {
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
  std::map<std::string, scoped_refptr<PolicyService> > new_policy_map;
  const DeviceLocalAccountList& list(
      device_settings.device_local_accounts().account());
  for (DeviceLocalAccountList::const_iterator account(list.begin());
       account != list.end(); ++account) {
    if (account->has_id()) {
      const std::string account_key = GetAccountKey(account->id());
      new_policy_map[account_key] = policy_map_[account_key];
    }
  }
  policy_map_.swap(new_policy_map);

  MigrateUppercaseDirs();

  // Purge all existing on-disk accounts that are no longer defined.
  file_util::FileEnumerator enumerator(device_local_account_dir_, false,
                                       file_util::FileEnumerator::DIRECTORIES);
  FilePath subdir;
  while (!(subdir = enumerator.Next()).empty()) {
    if (IsValidAccountKey(subdir.BaseName().value()) &&
        policy_map_.find(subdir.BaseName().value()) == policy_map_.end()) {
      LOG(INFO) << "Purging " << subdir.value();
      if (!file_util::Delete(subdir, true))
        LOG(ERROR) << "Failed to delete " << subdir.value();
    }
  }
}

bool DeviceLocalAccountPolicyService::MigrateUppercaseDirs(void) {
  file_util::FileEnumerator enumerator(device_local_account_dir_, false,
                                       file_util::FileEnumerator::DIRECTORIES);
  FilePath subdir;

  while (!(subdir = enumerator.Next()).empty()) {
    std::string upper = subdir.BaseName().value();
    std::string lower = StringToLowerASCII(upper);
    if (IsValidAccountKey(lower) && lower != upper) {
      FilePath subdir_to(subdir.DirName().Append(lower));
      LOG(INFO) << "Migrating " << upper << " to " << lower;
      if (!file_util::ReplaceFile(subdir, subdir_to))
        LOG(ERROR) << "Failed to migrate " << subdir.value();
    }
  }

  return true;
}

PolicyService* DeviceLocalAccountPolicyService::GetPolicyService(
    const std::string& account_id) {
  const std::string key = GetAccountKey(account_id);
  std::map<std::string, scoped_refptr<PolicyService> >::iterator entry =
      policy_map_.find(key);
  if (entry == policy_map_.end())
    return NULL;

  // Lazily create and initialize the policy service instance.
  if (!entry->second.get()) {
    const FilePath policy_path =
        device_local_account_dir_
            .AppendASCII(key)
            .Append(kPolicyDir)
            .Append(kPolicyFileName);
    if (!file_util::CreateDirectory(policy_path.DirName())) {
      LOG(ERROR) << "Failed to create directory for " << policy_path.value();
      return NULL;
    }

    scoped_ptr<PolicyStore> store(new PolicyStore(policy_path));
    if (!store->LoadOrCreate()) {
      // This is non-fatal, the policy may not have been stored yet.
      LOG(WARNING) << "Failed to load policy for device-local account "
                   << account_id;
    }
    entry->second =
        new PolicyService(store.Pass(), owner_key_, main_loop_);
  }

  return entry->second.get();
}

std::string DeviceLocalAccountPolicyService::GetAccountKey(
    const std::string& account_id) {
  return chromeos::cryptohome::home::SanitizeUserName(account_id);
}

bool DeviceLocalAccountPolicyService::IsValidAccountKey(
    const std::string& str) {
  return chromeos::cryptohome::home::IsSanitizedUserName(str);
}

}  // namespace
