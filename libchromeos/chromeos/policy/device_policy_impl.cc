// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device_policy_impl.h"

#include <openssl/evp.h>
#include <openssl/x509.h>

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "bindings/chrome_device_policy.pb.h"
#include "bindings/device_management_backend.pb.h"

namespace policy {

namespace {
const char kPolicyPath[] = "/var/lib/whitelist/policy";
const char kPublicKeyPath[] = "/var/lib/whitelist/owner.key";

// Reads the public key used to sign the policy from |key_file| and stores it
// in |public_key|. Returns true on success.
bool ReadPublicKeyFromFile(const FilePath& key_file, std::string* public_key) {
  if (!file_util::PathExists(key_file))
    return false;
  public_key->clear();
  if (!file_util::ReadFileToString(key_file, public_key) ||
      public_key->empty()) {
    LOG(ERROR) << "Could not read public key off disk";
    return false;
  }
  return true;
}

// Verifies that the |signed_data| has correct |signature| with |public_key|.
bool VerifySignature(std::string& signed_data,
                     std::string& signature,
                     std::string& public_key) {
  EVP_MD_CTX ctx;
  EVP_MD_CTX_init(&ctx);

  const EVP_MD* digest = EVP_sha1();

  char* key = const_cast<char*>(public_key.data());
  BIO* bio = BIO_new_mem_buf(key, public_key.length());
  if (!bio) {
    EVP_MD_CTX_cleanup(&ctx);
    return false;
  }

  EVP_PKEY* public_key_ssl = d2i_PUBKEY_bio(bio, NULL);
  if (!public_key_ssl) {
    BIO_free_all(bio);
    EVP_MD_CTX_cleanup(&ctx);
    return false;
  }

  const unsigned char* sig =
      reinterpret_cast<const unsigned char*>(signature.data());
  int rv = EVP_VerifyInit_ex(&ctx, digest, NULL);
  if (rv == 1) {
    EVP_VerifyUpdate(&ctx, signed_data.data(), signed_data.length());
    rv = EVP_VerifyFinal(&ctx,
                         sig, signature.length(),
                         public_key_ssl);
  }

  EVP_PKEY_free(public_key_ssl);
  BIO_free_all(bio);
  EVP_MD_CTX_cleanup(&ctx);

  return rv == 1;
}

}  // namespace

DevicePolicyImpl::DevicePolicyImpl()
    : policy_path_(kPolicyPath),
      keyfile_path_(kPublicKeyPath) {
}

DevicePolicyImpl::~DevicePolicyImpl() {
}

bool DevicePolicyImpl::LoadPolicy() {
  if (!VerifyPolicyFiles()) {
    return false;
  }

  std::string polstr;
  if (!file_util::ReadFileToString(policy_path_, &polstr) || polstr.empty()) {
    LOG(ERROR) << "Could not read policy off disk";
    return false;
  }
  if (!policy_.ParseFromString(polstr) || !policy_.has_policy_data()) {
    LOG(ERROR) << "Policy on disk could not be parsed!";
    return false;
  }
  policy_data_.ParseFromString(policy_.policy_data());
  if (!policy_data_.has_policy_value()) {
    LOG(ERROR) << "Policy on disk could not be parsed!";
    return false;
  }

  // Make sure the signature is still valid.
  if (!VerifyPolicySignature()) {
    LOG(ERROR) << "Policy signature verification failed!";
    return false;
  }

  device_policy_.ParseFromString(policy_data_.policy_value());
  return true;
}

// Writes the value of the PolicyRefreshRate policy in |rate|. Returns
// true on success.
bool DevicePolicyImpl::GetPolicyRefreshRate(int* rate) const {
  if (!device_policy_.has_device_policy_refresh_rate())
    return false;
  *rate = static_cast<int>(
      device_policy_.device_policy_refresh_rate().device_policy_refresh_rate());
  return true;
}

// Writes the value of the UserWhitelist policy in |user_whitelist|. Returns
// true on success.
bool DevicePolicyImpl::GetUserWhitelist(
    std::vector<std::string>* user_whitelist) const {
  if (!device_policy_.has_user_whitelist())
    return false;
  const enterprise_management::UserWhitelistProto& proto =
      device_policy_.user_whitelist();
  user_whitelist->clear();
  for (int i = 0; i < proto.user_whitelist_size(); i++)
    user_whitelist->push_back(proto.user_whitelist(i));
  return true;
}

// Writes the value of the GuestModeEnabled policy in |guest_mode_enabled|.
// Returns true on success.
bool DevicePolicyImpl::GetGuestModeEnabled(bool* guest_mode_enabled) const {
  if (!device_policy_.has_guest_mode_enabled())
    return false;
  *guest_mode_enabled =
      device_policy_.guest_mode_enabled().guest_mode_enabled();
  return true;
}

// Writes the value of the CameraEnabled policy in |camera_enabled|. Returns
// true on success.
bool DevicePolicyImpl::GetCameraEnabled(bool* camera_enabled) const {
  if (!device_policy_.has_camera_enabled())
    return false;
  *camera_enabled = device_policy_.camera_enabled().camera_enabled();
  return true;
}

// Writes the value of the ShowUserNamesOnSignIn policy in |show_usernames|.
// Returns true on success.
bool DevicePolicyImpl::GetShowUserNames(bool* show_user_names) const {
  if (!device_policy_.has_show_user_names())
    return false;
  *show_user_names = device_policy_.show_user_names().show_user_names();
  return true;
}

// Writes the value of the DataRoamingEnabled policy in |data_roaming_enabled|
// Returns true on success.
bool DevicePolicyImpl::GetDataRoamingEnabled(bool* data_roaming_enabled) const {
  if (!device_policy_.has_data_roaming_enabled())
    return false;
  *data_roaming_enabled =
      device_policy_.data_roaming_enabled().data_roaming_enabled();
  return true;
}

// Writes the value of the AllowNewUSer policy in |allow_new_user|. Returns
// true on success.
bool DevicePolicyImpl::GetAllowNewUsers(bool* allow_new_users) const {
  if (!device_policy_.has_allow_new_users())
    return false;
  *allow_new_users = device_policy_.allow_new_users().allow_new_users();
  return true;
}

// Writes the value of the MetricsEnabled policy in |metrics_enabled|. Returns
// true on success.
bool DevicePolicyImpl::GetMetricsEnabled(bool* metrics_enabled) const {
  if (!device_policy_.has_metrics_enabled())
    return false;
  *metrics_enabled = device_policy_.metrics_enabled().metrics_enabled();
  return true;
}

// Writes the value of ReportVersionInfo policy in |report_version_info|.
// Returns true on success.
bool DevicePolicyImpl::GetReportVersionInfo(bool* report_version_info) const {
  if (!device_policy_.has_device_reporting())
    return false;

  const enterprise_management::DeviceReportingProto& proto =
      device_policy_.device_reporting();
  if (!proto.has_report_version_info())
    return false;

  *report_version_info = proto.report_version_info();
  return true;
}

// Writes the value of ReportActivityTimes policy in |report_activity_times|.
// Returns true on success.
bool DevicePolicyImpl::GetReportActivityTimes(
    bool* report_activity_times) const {
  if (!device_policy_.has_device_reporting())
    return false;

  const enterprise_management::DeviceReportingProto& proto =
      device_policy_.device_reporting();
  if (!proto.has_report_activity_times())
    return false;

  *report_activity_times = proto.report_activity_times();
  return true;
}

// Writes the value of ReportBootMode policy in |report_boot_mode|. Returns
// true on success.
bool DevicePolicyImpl::GetReportBootMode(bool* report_boot_mode) const {
  if (!device_policy_.has_device_reporting())
    return false;

  const enterprise_management::DeviceReportingProto& proto =
      device_policy_.device_reporting();
  if (!proto.has_report_boot_mode())
    return false;

  *report_boot_mode = proto.report_boot_mode();
  return true;
}

// Writes the value of the EphemeralUsersEnabled policy in
// |ephemeral_users_enabled|. Returns true on success.
bool DevicePolicyImpl::GetEphemeralUsersEnabled(
    bool* ephemeral_users_enabled) const {
  if (!device_policy_.has_ephemeral_users_enabled())
    return false;
  *ephemeral_users_enabled =
      device_policy_.ephemeral_users_enabled().ephemeral_users_enabled();
  return true;
}

// Writes the value of the ProxyMode policy in |proxy_mode|. Returns true on
// success.
bool DevicePolicyImpl::GetProxyMode(std::string* proxy_mode) const {
  if (!device_policy_.has_device_proxy_settings())
    return false;

  const enterprise_management::DeviceProxySettingsProto& proto =
      device_policy_.device_proxy_settings();
  if (!proto.has_proxy_mode())
    return false;

  *proxy_mode = proto.proxy_mode();
  return true;
}

// Writes the value of the ProxyServer policy in |proxy_server|. Returns true
// on success.
bool DevicePolicyImpl::GetProxyServer(std::string* proxy_server) const {
  if (!device_policy_.has_device_proxy_settings())
    return false;

  const enterprise_management::DeviceProxySettingsProto& proto =
      device_policy_.device_proxy_settings();
  if (!proto.has_proxy_server())
    return false;

  *proxy_server = proto.proxy_server();
  return true;
}

// Writes the value of the ProxyPacUrl policy in |proxy_pac|. Returns true on
// success.
bool DevicePolicyImpl::GetProxyPacUrl(std::string* proxy_pac) const {
  if (!device_policy_.has_device_proxy_settings())
    return false;

  const enterprise_management::DeviceProxySettingsProto& proto =
      device_policy_.device_proxy_settings();
  if (!proto.has_proxy_pac_url())
    return false;

  *proxy_pac = proto.proxy_pac_url();
  return true;
}

// Writes the value of the ProxyBypassList policy in |proxy_bypass_list|.
// Returns true on success.
bool DevicePolicyImpl::GetProxyBypassList(
    std::string* proxy_bypass_list) const {
  if (!device_policy_.has_device_proxy_settings())
    return false;

  const enterprise_management::DeviceProxySettingsProto& proto =
      device_policy_.device_proxy_settings();
  if (!proto.has_proxy_bypass_list())
    return false;

  *proxy_bypass_list = proto.proxy_bypass_list();
  return true;
}

// Writes the value of the release channel policy in |release_channel|.
// Returns true on success.
bool DevicePolicyImpl::GetReleaseChannel(
    std::string* release_channel) const {
  if (!device_policy_.has_release_channel())
    return false;

  const enterprise_management::ReleaseChannelProto& proto =
      device_policy_.release_channel();
  if (!proto.has_release_channel())
    return false;

  *release_channel = proto.release_channel();
  return true;
}

// Writes the value of the update_disabled policy in |update_disabled|.
// Returns true on success.
bool DevicePolicyImpl::GetUpdateDisabled(
    bool* update_disabled) const {
  if (!device_policy_.has_auto_update_settings())
    return false;

  const enterprise_management::AutoUpdateSettingsProto& proto =
      device_policy_.auto_update_settings();
  if (!proto.has_update_disabled())
    return false;

  *update_disabled = proto.update_disabled();
  return true;
}

// Writes the value of the target_version_prefix policy in
// |target_version_prefix|. Returns true on success.
bool DevicePolicyImpl::GetTargetVersionPrefix(
    std::string* target_version_prefix) const {
  if (!device_policy_.has_auto_update_settings())
    return false;

  const enterprise_management::AutoUpdateSettingsProto& proto =
      device_policy_.auto_update_settings();
  if (!proto.has_target_version_prefix())
    return false;

  *target_version_prefix = proto.target_version_prefix();
  return true;
}

// Writes the value of the scatter_factor_in_seconds policy in
// |scatter_factor_in_seconds|. Returns true on success.
bool DevicePolicyImpl::GetScatterFactorInSeconds(
    int64* scatter_factor_in_seconds) const {
  if (!device_policy_.has_auto_update_settings())
    return false;

  const enterprise_management::AutoUpdateSettingsProto& proto =
      device_policy_.auto_update_settings();
  if (!proto.has_scatter_factor_in_seconds())
    return false;

  *scatter_factor_in_seconds = proto.scatter_factor_in_seconds();
  return true;
}

// Writes the value of the OpenNetworkConfiguration policy in
// |open_network_configuration|. Returns true on success.
bool DevicePolicyImpl::GetOpenNetworkConfiguration(
    std::string* open_network_configuration) const {
  if (!device_policy_.has_open_network_configuration())
    return false;

  const enterprise_management::DeviceOpenNetworkConfigurationProto& proto =
      device_policy_.open_network_configuration();
  if (!proto.has_open_network_configuration())
    return false;

  *open_network_configuration = proto.open_network_configuration();
  return true;
}

// Writes the name of the device owner in |owner|. For enterprise enrolled
// devices, this will be an empty string.
// Returns true on success.
bool DevicePolicyImpl::GetOwner(std::string* owner) const {
  // The device is enterprise enrolled iff a request token exists.
  if (policy_data_.has_request_token()) {
    *owner = "";
    return true;
  }
  if (!policy_data_.has_username())
    return false;
  *owner = policy_data_.username();
  return true;
}

bool DevicePolicyImpl::VerifyPolicyFiles() {
  // Both the policy and its signature have to exist.
  if (!file_util::PathExists(policy_path_) ||
      !file_util::PathExists(keyfile_path_)) {
    return false;
  }

  // Check if the policy and signature file is owned by root.
  struct stat file_stat;
  stat(policy_path_.value().c_str(), &file_stat);
  if (file_stat.st_uid != 0) {
    LOG(ERROR) << "Policy file is not owned by root!";
    return false;
  }
  stat(keyfile_path_.value().c_str(), &file_stat);
  if (file_stat.st_uid != 0) {
    LOG(ERROR) << "Policy signature file is not owned by root!";
    return false;
  }
  return true;
}

bool DevicePolicyImpl::VerifyPolicySignature() {
  if (policy_.has_policy_data_signature()) {
    std::string policy_data = policy_.policy_data();
    std::string policy_data_signature = policy_.policy_data_signature();
    std::string public_key;
    if (!ReadPublicKeyFromFile(FilePath(keyfile_path_), &public_key)) {
      LOG(ERROR) << "Could not read owner key off disk";
      return false;
    }
    if (!VerifySignature(policy_data, policy_data_signature, public_key)) {
      LOG(ERROR) << "Signature does not match the data or can not be verified!";
      return false;
    }
    return true;
  }
  LOG(ERROR) << "The policy blob is not signed!";
  return false;
}

}  // namespace policy

