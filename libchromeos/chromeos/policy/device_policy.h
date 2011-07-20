// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBPOLICY_DEVICE_POLICY_H_
#define LIBPOLICY_DEVICE_POLICY_H_

#include <string>
#include <vector>

#include <base/basictypes.h>

class FilePath;

namespace policy {

// This class holds device settings that are to be enforced across all users.
// It is also responsible for loading the policy blob from disk and verifying
// the signature against the owner's key.
//
// This class defines the interface for queryng device policy on ChromeOS.
// The implementation is hidden in DevicePolicyImpl to prevent protobuf
// definition from leaking into the libraries using this interface.
class DevicePolicy {
 public:
  DevicePolicy();
  virtual ~DevicePolicy();

  // Load the signed policy off of disk into |policy_|.
  // Returns true unless there is a policy on disk and loading it fails.
  virtual bool LoadPolicy() = 0;

  // Writes the value of the DevicePolicyRefreshRate policy in |rate|. Returns
  // true on success.
  virtual bool GetPolicyRefreshRate(int* rate) const = 0;

  // Writes the value of the UserWhitelist policy in |user_whitelist|. Returns
  // true on success.
  virtual bool GetUserWhitelist(
      std::vector<std::string>* user_whitelist) const = 0;

  // Writes the value of the GuestModeEnabled policy in |guest_mode_enabled|.
  // Returns true on success.
  virtual bool GetGuestModeEnabled(bool* guest_mode_enabled) const = 0;

  // Writes the value of the CameraEnabled policy in |camera_enabled|. Returns
  // true on success.
  virtual bool GetCameraEnabled(bool* camera_enabled) const = 0;

  // Writes the value of the ShowUserNamesOnSignIn policy in |show_user_names|.
  // Returns true on success.
  virtual bool GetShowUserNames(bool* show_user_names) const = 0;

  // Writes the value of the DataRoamingEnabled policy in |data_roaming_enabled|
  // Returns true on success.
  virtual bool GetDataRoamingEnabled(bool* data_roaming_enabled) const = 0;

  // Writes the value of the AllowNewUsers policy in |allow_new_users|. Returns
  // true on success.
  virtual bool GetAllowNewUsers(bool* allow_new_users) const = 0;

  // Writes the value of MetricEnabled policy in |metrics_enabled|. Returns true
  // on success.
  virtual bool GetMetricsEnabled(bool* metrics_enabled) const = 0;

  // Writes the value of the ProxyMode policy in |proxy_mode|. Returns true on
  // success.
  virtual bool GetProxyMode(std::string* proxy_mode) const = 0;

  // Writes the value of the ProxyServer policy in |proxy_server|. Returns true
  // on success.
  virtual bool GetProxyServer(std::string* proxy_server) const = 0;

  // Writes the value of the ProxyPacUrl policy in |proxy_pac|. Returns true on
  // success.
  virtual bool GetProxyPacUrl(std::string* proxy_pac) const = 0;

  // Writes the value of the ProxyBypassList policy in |proxy_bypass_list|.
  // Returns true on success.
  virtual bool GetProxyBypassList(std::string* proxy_bypass_list) const = 0;

 private:
  // Verifies that the policy files are owned by root and exist.
  virtual bool VerifyPolicyFiles() = 0;

  // Verifies that the policy signature is correct.
  virtual bool VerifyPolicySignature() = 0;

  DISALLOW_COPY_AND_ASSIGN(DevicePolicy);
};
}  // namespace policy

#endif  // LIBPOLICY_DEVICE_POLICY_H_
