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

  // Load the signed policy off of disk into |policy_|.
  // Returns true unless there is a policy on disk and loading it fails.
  virtual bool LoadPolicy();

  // Writes the value of the DevicePolicyRefreshRate policy in |rate|. Returns
  // true on success.
  virtual bool GetPolicyRefreshRate(int* rate) const;

  // Writes the value of the UserWhitelist policy in |user_whitelist|. Returns
  // true on success.
  virtual bool GetUserWhitelist(std::vector<std::string>* user_whitelist) const;

  // Writes the value of the GuestModeEnabled policy in |guest_mode_enabled|.
  // Returns true on success.
  virtual bool GetGuestModeEnabled(bool* guest_mode_enabled) const;

  // Writes the value of the CameraEnabled policy in |camera_enabled|. Returns
  // true on success.
  virtual bool GetCameraEnabled(bool* camera_enabled) const;

  // Writes the value of the ShowUserNamesOnSignIn policy in |show_user_names|.
  // Returns true on success.
  virtual bool GetShowUserNames(bool* show_user_names) const;

  // Writes the value of the DataRoamingEnabled policy in |data_roaming_enabled|
  // Returns true on success.
  virtual bool GetDataRoamingEnabled(bool* data_roaming_enabled) const;

  // Writes the value of the AllowNewUsers policy in |allow_new_users|. Returns
  // true on success.
  virtual bool GetAllowNewUsers(bool* allow_new_users) const;

  // Writes the value of MetricEnabled policy in |metrics_enabled|. Returns true
  // on success.
  virtual bool GetMetricsEnabled(bool* metrics_enabled) const;

  // Writes the value of ReportVersionInfo policy in |report_version_info|.
  // Returns true on success.
  virtual bool GetReportVersionInfo(bool* report_version_info) const;

  // Writes the value of ReportActivityTimes policy in |report_activity_times|.
  // Returns true on success.
  virtual bool GetReportActivityTimes(bool* report_activity_times) const;

  // Writes the value of ReportBootMode policy in |report_boot_mode|. Returns
  // true on success.
  virtual bool GetReportBootMode(bool* report_boot_mode) const;

  // Writes the value of the EphemeralUsersEnabled policy in
  // |ephemeral_users_enabled|. Returns true on success.
  virtual bool GetEphemeralUsersEnabled(bool* ephemeral_users_enabled) const;

  // Writes the value of the ProxyMode policy in |proxy_mode|. Returns true on
  // success.
  virtual bool GetProxyMode(std::string* proxy_mode) const;

  // Writes the value of the ProxyServer policy in |proxy_server|. Returns true
  // on success.
  virtual bool GetProxyServer(std::string* proxy_server) const;

  // Writes the value of the ProxyPacUrl policy in |proxy_pac|. Returns true on
  // success.
  virtual bool GetProxyPacUrl(std::string* proxy_pac) const;

  // Writes the value of the ProxyBypassList policy in |proxy_bypass_list|.
  // Returns true on success.
  virtual bool GetProxyBypassList(std::string* proxy_bypass_list) const;

  // Writes the value of the release channel policy in |release channel|.
  // Returns true on success.
  virtual bool GetReleaseChannel(std::string* release_channel) const;

  // Writes the value of the update_disabled policy in |update_disabled|.
  // Returns true on success.
  virtual bool GetUpdateDisabled(bool* update_disabled) const;

  // Writes the value of the target_version_prefix policy in
  // |target_version_prefix|. Returns true on success.
  virtual bool GetTargetVersionPrefix(
      std::string* target_version_prefix) const;

  // Writes the value of the scatter_factor_in_seconds policy in
  // |scatter_factor_in_seconds|. Returns true on success.
  virtual bool GetScatterFactorInSeconds(
      int64* scatter_factor_in_seconds) const;

  // Writes the value of the OpenNetworkConfiguration policy in
  // |open_network_configuration|. Returns true on success.
  virtual bool GetOpenNetworkConfiguration(
      std::string* open_network_configuration) const;

  // Writes the name of the device owner in |owner|. For enterprise enrolled
  // devices, this will be an empty string.
  // Returns true on success.
  virtual bool GetOwner(std::string* owner) const;

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

#endif  // LIBPOLICY_DEVICE_POLICY_IMPL_H_
