// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBPOLICY_DEVICE_POLICY_IMPL_H_
#define LIBPOLICY_DEVICE_POLICY_IMPL_H_

#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/file_path.h>

#include "bindings/chrome_device_policy.pb.h"
#include "bindings/device_management_backend.pb.h"
#include "device_policy.h"

#pragma GCC visibility push(default)

namespace policy {

// This class holds device settings that are to be enforced across all users.
//
// Before serving it to the users this class verifies that the policy is valid
// against its signature and the owner's key and also that the policy files
// are owned by root.
class DevicePolicyImpl : public DevicePolicy {
 public:
  DevicePolicyImpl();
  virtual ~DevicePolicyImpl();

  virtual bool LoadPolicy();
  virtual bool GetPolicyRefreshRate(int* rate) const;
  virtual bool GetUserWhitelist(std::vector<std::string>* user_whitelist) const;
  virtual bool GetGuestModeEnabled(bool* guest_mode_enabled) const;
  virtual bool GetCameraEnabled(bool* camera_enabled) const;
  virtual bool GetShowUserNames(bool* show_user_names) const;
  virtual bool GetDataRoamingEnabled(bool* data_roaming_enabled) const;
  virtual bool GetAllowNewUsers(bool* allow_new_users) const;
  virtual bool GetMetricsEnabled(bool* metrics_enabled) const;
  virtual bool GetReportVersionInfo(bool* report_version_info) const;
  virtual bool GetReportActivityTimes(bool* report_activity_times) const;
  virtual bool GetReportBootMode(bool* report_boot_mode) const;
  virtual bool GetEphemeralUsersEnabled(bool* ephemeral_users_enabled) const;
  virtual bool GetProxyMode(std::string* proxy_mode) const;
  virtual bool GetProxyServer(std::string* proxy_server) const;
  virtual bool GetProxyPacUrl(std::string* proxy_pac) const;
  virtual bool GetProxyBypassList(std::string* proxy_bypass_list) const;
  virtual bool GetReleaseChannel(std::string* release_channel) const;
  virtual bool GetReleaseChannelDelegated(
      bool* release_channel_delegated) const;
  virtual bool GetUpdateDisabled(bool* update_disabled) const;
  virtual bool GetTargetVersionPrefix(
      std::string* target_version_prefix) const;
  virtual bool GetScatterFactorInSeconds(
      int64* scatter_factor_in_seconds) const;
  virtual bool GetAllowedConnectionTypesForUpdate(
      std::set<std::string>* connection_types) const;
  virtual bool GetOpenNetworkConfiguration(
      std::string* open_network_configuration) const;
  virtual bool GetOwner(std::string* owner) const;
  virtual bool GetHttpDownloadsEnabled(bool* http_downloads_enabled) const;

 protected:
  // Verifies that the policy files are owned by root and exist.
  virtual bool VerifyPolicyFiles();

  FilePath policy_path_;
  FilePath keyfile_path_;

 private:
  // Verifies that the policy signature is correct.
  virtual bool VerifyPolicySignature();

  enterprise_management::PolicyFetchResponse policy_;
  enterprise_management::PolicyData policy_data_;
  enterprise_management::ChromeDeviceSettingsProto device_policy_;

  DISALLOW_COPY_AND_ASSIGN(DevicePolicyImpl);
};
}  // namespace policy

#pragma GCC visibility pop

#endif  // LIBPOLICY_DEVICE_POLICY_IMPL_H_
