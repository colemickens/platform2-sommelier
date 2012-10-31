// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_DEVICE_LOCAL_ACCOUNT_POLICY_SERVICE_H_
#define LOGIN_MANAGER_DEVICE_LOCAL_ACCOUNT_POLICY_SERVICE_H_

#include <map>
#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/file_path.h>
#include <base/memory/ref_counted.h>

#include "login_manager/policy_service.h"

namespace base {
class MessageLoopProxy;
}

namespace enterprise_management {
class ChromeDeviceSettingsProto;
}

namespace login_manager {

class PolicyKey;

// Manages policy blobs for device-local accounts, loading/storing them from/to
// disk, making sure signature checks are performed on store operations and
// restricting access to the accounts defined in device settings.
class DeviceLocalAccountPolicyService {
 public:
  // Name of the subdirectory to store policy in.
  static const FilePath::CharType kPolicyDir[];
  // File name of the file within |kPolicyDir| that holds the policy blob.
  static const FilePath::CharType kPolicyFileName[];

  DeviceLocalAccountPolicyService(
      const FilePath& device_local_account_dir,
      PolicyKey* owner_key,
      const scoped_refptr<base::MessageLoopProxy>& main_loop);
  ~DeviceLocalAccountPolicyService();

  // Store policy for |account_id|, return false if the device-local account is
  // not defined in device policy.
  bool Store(const std::string& account_id,
             const uint8* policy_data,
             uint32 policy_data_size,
             PolicyService::Completion* completion);

  // Load policy for a given |account_id| and places the result in
  // |policy_data|. Returns true if the account exists and policy could be read
  // successfully, false otherwise.
  bool Retrieve(const std::string& account_id,
                std::vector<uint8>* policy_data);

  // Updates device settings, i.e. what device-local accounts are available.
  // This will purge any on-disk state for accounts that are no longer defined
  // in device settings. Later requests to Load() and Store() will respect the
  // new list of device-local accounts and fail for accounts that are not
  // present.
  void UpdateDeviceSettings(
      const enterprise_management::ChromeDeviceSettingsProto& device_settings);

 private:
  // Obtains the PolicyService instance that manages disk storage for
  // |account_id| after checking that |account_id| is valid. The PolicyService
  // is lazily created on the fly if not present yet.
  PolicyService* GetPolicyService(const std::string& account_id);

  // Returns the identifier for a given |account_id|. The value returned is safe
  // to use as a file system name. This may fail, in which case the returned
  // string will be empty.
  std::string GetAccountKey(const std::string& account_id);

  // Checks whether the passed string is a properly formatted account key.
  bool IsValidAccountKey(const std::string& str);

  // The base path for storing device-local account information on disk.
  const FilePath device_local_account_dir_;

  // The policy key to verify signatures against.
  PolicyKey* owner_key_;

  // Main loop for the policy service to use.
  scoped_refptr<base::MessageLoopProxy> main_loop_;

  // Keeps lazily-created instances of the device-local account policy services.
  // The keys present in this map are kept in sync with device policy. Entries
  // that are not present are invalid, entries that contain a NULL pointer
  // indicate the respective policy blob hasn't been pulled from disk yet.
  std::map<std::string, scoped_refptr<PolicyService> > policy_map_;

  DISALLOW_COPY_AND_ASSIGN(DeviceLocalAccountPolicyService);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_DEVICE_LOCAL_ACCOUNT_POLICY_SERVICE_H_
