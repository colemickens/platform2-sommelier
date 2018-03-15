// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "authpolicy/samba_interface.h"

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <base/files/file.h>
#include <base/files/file_util.h>
#include <base/files/important_file_writer.h>
#include <base/memory/ptr_util.h>
#include <base/single_thread_task_runner.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <base/threading/platform_thread.h>
#include <base/time/time.h>
#include <policy/device_policy_impl.h>

#include "authpolicy/anonymizer.h"
#include "authpolicy/platform_helper.h"
#include "authpolicy/process_executor.h"

namespace em = enterprise_management;

namespace authpolicy {
namespace {

// Samba configuration file data.
const char kSmbConfData[] =
    "[global]\n"
    "\tnetbios name = %s\n"
    "\tsecurity = ADS\n"
    "\tworkgroup = %s\n"
    "\trealm = %s\n"
    "\tlock directory = %s\n"
    "\tcache directory = %s\n"
    "\tstate directory = %s\n"
    "\tprivate directory = %s\n"
    "\tkerberos encryption types = %s\n"
    "\tclient signing = mandatory\n"
    "\tclient min protocol = SMB2\n"
    // TODO(ljusten): Remove this line once crbug.com/662440 is resolved.
    "\tclient max protocol = SMB3\n"
    "\tclient ipc min protocol = SMB2\n"
    "\tclient schannel = yes\n"
    "\tclient ldap sasl wrapping = sign\n";

const int kFileMode_rwr = base::FILE_PERMISSION_READ_BY_USER |
                          base::FILE_PERMISSION_WRITE_BY_USER |
                          base::FILE_PERMISSION_READ_BY_GROUP;

const int kFileMode_rwxrx = kFileMode_rwr |
                            base::FILE_PERMISSION_EXECUTE_BY_USER |
                            base::FILE_PERMISSION_EXECUTE_BY_GROUP;

const int kFileMode_rwxrwx =
    kFileMode_rwxrx | base::FILE_PERMISSION_WRITE_BY_GROUP;

// Directories with permissions to be created. AUTHPOLICY_TMP_DIR needs group rx
// access to read smb.conf and krb5.conf and to access SAMBA_DIR, but no write
// access. The Samba directories need full group rwx access since Samba reads
// and writes files there.
const std::pair<Path, int> kDirsAndMode[] = {
    {Path::TEMP_DIR, kFileMode_rwxrx},
    {Path::SAMBA_DIR, kFileMode_rwxrwx},
    {Path::SAMBA_LOCK_DIR, kFileMode_rwxrwx},
    {Path::SAMBA_CACHE_DIR, kFileMode_rwxrwx},
    {Path::SAMBA_STATE_DIR, kFileMode_rwxrwx},
    {Path::SAMBA_PRIVATE_DIR, kFileMode_rwxrwx}};

// Directory / filenames for user and device policy.
const char kPRegUserDir[] = "User";
const char kPRegDeviceDir[] = "Machine";
const char kPRegFileName[] = "registry.pol";

// Size limit when loading the config file (256 kb).
const size_t kConfigSizeLimit = 256 * 1024;

// Maximum smbclient tries.
const int kSmbClientMaxTries = 5;
// Wait interval between two smbclient tries.
const int kSmbClientRetryWaitSeconds = 1;

// Check every 120 minutes whether the machine password has to be changed.
const int kPasswordChangeCheckRateMinutes = 120;

// Keys for interpreting net output.
const char kKeyJoinAccessDenied[] = "NT_STATUS_ACCESS_DENIED";
const char kKeyInvalidMachineName[] = "Improperly formed account name";
const char kKeyMachineNameTooLong[] = "Our netbios name can be at most";
const char kKeyUserHitJoinQuota[] =
    "Insufficient quota exists to complete the operation";
const char kKeyJoinFailedToFindDC[] = "failed to find DC";
const char kKeyNoLogonServers[] = "No logon servers";
const char kKeyJoinLogonFailure[] = "Logon failure";
const char kKeyJoinMustChangePassword[] = "Must change password";
// Setting OU during domain join failed. More specific errors below.
const char kKeyBadOuCommon[] = "failed to precreate account in ou";
// The domain join createcomputer argument specified a non-existent OU.
const char kKeyBadOuNoSuchObject[] = "No such object";
// The domain join createcomputer argument syntax was invalid. Caused by some
// special characters in OU names, e.g. 'ou=123!' or 'a"b'. Seems like a Samba
// issue since OUs allow all characters and we do escape names properly.
const char kKeyBadOuInvalidDnSyntax[] = "Invalid DN syntax";
// Domain join operation would have violated an attribute constraint.
const char kKeyBadOuConstrainViolation[] = "Constraint violation";
// Domain join required access permissions that the user does not possess.
const char kKeyBadOuInsufficientAccess[] = "Insufficient access";
// All other OU errors result in a generic ERROR_SETTING_OU_FAILED, e.g.
//  - "Referral": dc=... specification resulted in a referral to another server.
//  - "Operations error": Unspecific error.
// Keys for interpreting smbclient output.
const char kKeyConnectionReset[] = "NT_STATUS_CONNECTION_RESET";
const char kKeyNetworkTimeout[] = "NT_STATUS_IO_TIMEOUT";
const char kKeyObjectNameNotFound[] =
    "NT_STATUS_OBJECT_NAME_NOT_FOUND opening remote file ";
const char kKeyEncTypeNotSupported[] = "KDC has no support for encryption type";

// Replacement strings for anonymization.
const char kMachineNamePlaceholder[] = "<MACHINE_NAME>";
const char kMachinePassPlaceholder[] = "<MACHINE_PASS>";
const char kLogonNamePlaceholder[] = "<USER_LOGON_NAME>";
const char kGivenNamePlaceholder[] = "<USER_GIVEN_NAME>";
const char kDisplayNamePlaceholder[] = "<USER_DISPLAY_NAME>";
const char kSAMAccountNamePlaceholder[] = "<USER_SAM_ACCOUNT_NAME>";
const char kCommonNamePlaceholder[] = "<USER_COMMON_NAME>";
const char kAccountIdPlaceholder[] = "<USER_ACCOUNT_ID>";
const char kWorkgroupPlaceholder[] = "<WORKGROUP>";
const char kDeviceRealmPlaceholder[] = "<DEVICE_REALM>";
const char kUserRealmPlaceholder[] = "<USER_REALM>";
const char kForestPlaceholder[] = "<FOREST>";
const char kDomainPlaceholder[] = "<DOMAIN>";
const char kServerNamePlaceholder[] = "<SERVER_NAME>";
const char kSiteNamePlaceholder[] = "<SITE_NAME>";
const char kIpAddressPlaceholder[] = "<IP_ADDRESS>";

// Keys for net ads searches.
const char kKeyWorkgroup[] = "Workgroup";
const char kKeyAdsDnsParseRrSrv[] = "ads_dns_parse_rr_srv";
const char kKeyPdcDnsName[] = "pdc_dns_name";
const char kKeyAdsDcName[] = "ads_dc_name";
const char kKeyPdcName[] = "pdc_name";
const char kKeyServerSite[] = "server_site";
const char kKeyClientSite[] = "client_site";
const char kKeyForest[] = "Forest";
const char kKeyDomain[] = "Domain";
const char kKeyDomainController[] = "Domain Controller";
const char kKeyPreWin2kDomain[] = "Pre-Win2k Domain";
const char kKeyPreWin2kHostname[] = "Pre-Win2k Hostname";
const char kKeyServerSiteName[] = "Server Site Name";
const char kKeyClientSiteName[] = "Client Site Name";
const char kKeyKdcServer[] = "KDC server";
const char kKeyLdapServer[] = "LDAP server";
const char kKeyLdapServerName[] = "LDAP server name";

// Kerberos encryption types strings for smb.conf.
constexpr char kEncTypesAll[] = "all";
constexpr char kEncTypesStrong[] = "strong";
constexpr char kEncTypesLegacy[] = "legacy";

// Maximum time that logging through SetDefaultLogLevel() should stay enabled.
// The method is called through the authpolicy_debug crosh command. The time is
// limited so users don't have to remember to turn logging off.
// Keep in sync with description in crosh!
int kMaxDefaultLogLevelUptimeMinutes = 30;

ErrorType GetNetError(const ProcessExecutor& executor,
                      const std::string& net_command) {
  const std::string& net_out = executor.GetStdout();
  const std::string& net_err = executor.GetStderr();
  const std::string error_msg("net ads " + net_command + " failed: ");

  if (Contains(net_out, kKeyJoinFailedToFindDC) ||
      Contains(net_err, kKeyNoLogonServers)) {
    LOG(ERROR) << error_msg << "network problem";
    return ERROR_NETWORK_PROBLEM;
  }
  if (Contains(net_out, kKeyJoinLogonFailure)) {
    LOG(ERROR) << error_msg << "logon failure";
    return ERROR_BAD_PASSWORD;
  }
  if (Contains(net_out, kKeyJoinMustChangePassword)) {
    LOG(ERROR) << error_msg << "must change password";
    return ERROR_PASSWORD_EXPIRED;
  }
  if (Contains(net_out, kKeyJoinAccessDenied)) {
    LOG(ERROR) << error_msg << "user is not permitted to join the domain";
    return ERROR_JOIN_ACCESS_DENIED;
  }
  if (Contains(net_out, kKeyInvalidMachineName)) {
    LOG(ERROR) << error_msg << "invalid machine name";
    return ERROR_INVALID_MACHINE_NAME;
  }
  if (Contains(net_out, kKeyMachineNameTooLong)) {
    LOG(ERROR) << error_msg << "machine name is too long";
    return ERROR_MACHINE_NAME_TOO_LONG;
  }
  if (Contains(net_out, kKeyUserHitJoinQuota)) {
    LOG(ERROR) << error_msg << "user joined max number of machines";
    return ERROR_USER_HIT_JOIN_QUOTA;
  }
  if (Contains(net_out, kKeyBadOuCommon)) {
    if (Contains(net_out, kKeyBadOuNoSuchObject)) {
      LOG(ERROR) << error_msg << "computer OU does not exist";
      return ERROR_OU_DOES_NOT_EXIST;
    }
    if (Contains(net_out, kKeyBadOuInvalidDnSyntax)) {
      LOG(ERROR) << error_msg << "computer OU invalid";
      return ERROR_INVALID_OU;
    }
    if (Contains(net_out, kKeyBadOuConstrainViolation) ||
        Contains(net_out, kKeyBadOuInsufficientAccess)) {
      LOG(ERROR) << error_msg << "access denied setting computer OU";
      return ERROR_OU_ACCESS_DENIED;
    }
    LOG(ERROR) << error_msg << "setting computer OU failed, unspecified error";
    return ERROR_SETTING_OU_FAILED;
  }
  if (Contains(net_out, kKeyEncTypeNotSupported)) {
    LOG(ERROR) << error_msg << "KDC does not support encryption type";
    return ERROR_KDC_DOES_NOT_SUPPORT_ENCRYPTION_TYPE;
  }
  LOG(ERROR) << error_msg << "exit code " << executor.GetExitCode();
  return ERROR_NET_FAILED;
}

ErrorType GetSmbclientError(const ProcessExecutor& smb_client_cmd) {
  const std::string& smb_client_out = smb_client_cmd.GetStdout();
  if (Contains(smb_client_out, kKeyNetworkTimeout) ||
      Contains(smb_client_out, kKeyConnectionReset)) {
    LOG(ERROR) << "smbclient failed - network problem";
    return ERROR_NETWORK_PROBLEM;
  }
  LOG(ERROR) << "smbclient failed with exit code "
             << smb_client_cmd.GetExitCode();
  return ERROR_SMBCLIENT_FAILED;
}

// Creates the given directory recursively and sets error message on failure.
ErrorType CreateDirectory(const base::FilePath& dir) {
  base::File::Error ferror;
  if (!base::CreateDirectoryAndGetError(dir, &ferror)) {
    LOG(ERROR) << "Failed to create directory '" << dir.value()
               << "': " << base::File::ErrorToString(ferror);
    return ERROR_LOCAL_IO;
  }
  return ERROR_NONE;
}

// Sets file permissions for a given filepath and sets error message on failure.
ErrorType SetFilePermissions(const base::FilePath& fp, int mode) {
  if (!base::SetPosixFilePermissions(fp, mode)) {
    LOG(ERROR) << "Failed to set permissions on '" << fp.value() << "'";
    return ERROR_LOCAL_IO;
  }
  return ERROR_NONE;
}

// Similar to |SetFilePermissions|, but sets permissions recursively up the path
// to |base_fp| (not including |base_fp|). Returns false if |base_fp| is not a
// parent of |fp|.
ErrorType SetFilePermissionsRecursive(const base::FilePath& fp,
                                      const base::FilePath& base_fp,
                                      int mode) {
  if (!base_fp.IsParent(fp)) {
    LOG(ERROR) << "Base path '" << base_fp.value() << "' is not a parent of '"
               << fp.value() << "'";
    return ERROR_LOCAL_IO;
  }
  ErrorType error = ERROR_NONE;
  for (base::FilePath curr_fp = fp; curr_fp != base_fp && error == ERROR_NONE;
       curr_fp = curr_fp.DirName()) {
    error = SetFilePermissions(curr_fp, mode);
  }
  return error;
}

// Checks whether the file at |default_level_path| exists and was last modified
// in a certain time range. If not, it is deleted to prevent that a user forgets
// to disable logging.
bool CheckFlagsDefaultLevelValid(const base::FilePath& default_level_path) {
  // Having no file is the out-of-box state with no level set, so exit quietly.
  if (!base::PathExists(default_level_path))
    return false;

  base::File::Info info;
  if (!GetFileInfo(default_level_path, &info)) {
    PLOG(ERROR) << "Failed to get file info from '"
                << default_level_path.value() << "'";
    return false;
  }

  // Check < -1 to prevent issues with clocks running backwards for a bit.
  int uptime_min = (base::Time::Now() - info.last_modified).InMinutes();
  if (uptime_min < -1 || uptime_min > kMaxDefaultLogLevelUptimeMinutes) {
    LOG(INFO) << "Removing flags default level file and resetting (uptime: "
              << uptime_min << " minutes).";
    PCHECK(base::DeleteFile(default_level_path, false /* recursive */))
        << "Failed to delete flags default level file '"
        << default_level_path.value() << "'";
    return false;
  }

  return true;
}

// Parses |gpo_policy_data| from |gpo_policy_data_blob|. Returns ERROR_NONE on
// success. Returns ERROR_PARSE_FAILED and prints an error on failure.
ErrorType ParsePolicyData(const std::string& gpo_policy_data_blob,
                          protos::GpoPolicyData* gpo_policy_data) {
  if (!gpo_policy_data->ParseFromString(gpo_policy_data_blob)) {
    LOG(ERROR) << "Failed to parse policy data from string";
    return ERROR_PARSE_FAILED;
  }
  return ERROR_NONE;
}

// Returns the string representation of |encryption_types| for smb.conf.
const char* GetEncryptionTypesString(KerberosEncryptionTypes encryption_types) {
  switch (encryption_types) {
    case ENC_TYPES_ALL:
      return kEncTypesAll;
    case ENC_TYPES_STRONG:
      return kEncTypesStrong;
    case ENC_TYPES_LEGACY:
      return kEncTypesLegacy;
  }
  CHECK(false);
}

// Gets the Kerberos encryption types from the corresponding device policy.
// Returns ENC_TYPES_STRONG if the policy is not set or invalid.
KerberosEncryptionTypes GetEncryptionTypes(
    const em::ChromeDeviceSettingsProto& device_policy) {
  if (!device_policy.has_device_kerberos_encryption_types() ||
      !device_policy.device_kerberos_encryption_types().has_types()) {
    return ENC_TYPES_STRONG;
  }

  em::DeviceKerberosEncryptionTypesProto::Types policy_encryption_types =
      device_policy.device_kerberos_encryption_types().types();

  switch (policy_encryption_types) {
    case em::DeviceKerberosEncryptionTypesProto::ENC_TYPES_ALL:
      return ENC_TYPES_ALL;
    case em::DeviceKerberosEncryptionTypesProto::ENC_TYPES_STRONG:
      return ENC_TYPES_STRONG;
    case em::DeviceKerberosEncryptionTypesProto::ENC_TYPES_LEGACY:
      return ENC_TYPES_LEGACY;
  }

  CHECK(false);
}

// Gets the user policy loopback processing mode the corresponding device
// policy. Returns USER_POLICY_MODE_DEFAULT if the policy is not.
em::DeviceUserPolicyLoopbackProcessingModeProto::Mode GetUserPolicyMode(
    const em::ChromeDeviceSettingsProto& device_policy) {
  if (!device_policy.has_device_user_policy_loopback_processing_mode() ||
      !device_policy.device_user_policy_loopback_processing_mode().has_mode()) {
    return em::DeviceUserPolicyLoopbackProcessingModeProto::
        USER_POLICY_MODE_DEFAULT;
  }
  return device_policy.device_user_policy_loopback_processing_mode().mode();
}

// Gets the user policy loopback processing mode the corresponding device
// policy. Returns a time delta of |kDefaultMachinePasswordChangeRateDays| days
// if the policy is not.
base::TimeDelta GetMachinePasswordChangeRate(
    const em::ChromeDeviceSettingsProto& device_policy) {
  if (!device_policy.has_device_machine_password_change_rate() ||
      !device_policy.device_machine_password_change_rate().has_rate_days()) {
    return base::TimeDelta::FromDays(kDefaultMachinePasswordChangeRateDays);
  }
  return base::TimeDelta::FromDays(
      device_policy.device_machine_password_change_rate().rate_days());
}

}  // namespace

SambaInterface::SambaInterface(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    AuthPolicyMetrics* metrics,
    const PathService* path_service,
    const base::Closure& user_kerberos_files_changed)
    : user_account_(Path::USER_SMB_CONF),
      device_account_(Path::DEVICE_SMB_CONF),
      metrics_(metrics),
      paths_(path_service),
      anonymizer_(std::make_unique<Anonymizer>()),
      jail_helper_(paths_, &flags_, anonymizer_.get()),
      user_tgt_manager_(task_runner,
                        paths_,
                        metrics_,
                        &flags_,
                        &jail_helper_,
                        anonymizer_.get(),
                        Path::USER_KRB5_CONF,
                        Path::USER_CREDENTIAL_CACHE),
      device_tgt_manager_(task_runner,
                          paths_,
                          metrics_,
                          &flags_,
                          &jail_helper_,
                          anonymizer_.get(),
                          Path::DEVICE_KRB5_CONF,
                          Path::DEVICE_CREDENTIAL_CACHE) {
  DCHECK(paths_);
  LoadFlagsDefaultLevel();
  user_tgt_manager_.SetKerberosFilesChangedCallback(
      user_kerberos_files_changed);
}

SambaInterface::~SambaInterface() = default;

ErrorType SambaInterface::Initialize(bool expect_config) {
  ReloadDebugFlags();

  ErrorType error = ERROR_NONE;
  for (const auto& dir_and_mode : kDirsAndMode) {
    const base::FilePath dir(paths_->Get(dir_and_mode.first));
    const int mode = dir_and_mode.second;
    error = ::authpolicy::CreateDirectory(dir);
    if (error != ERROR_NONE)
      return error;
    error = SetFilePermissions(dir, mode);
    if (error != ERROR_NONE)
      return error;
  }

  if (expect_config) {
    error = ReadConfiguration();
    if (error != ERROR_NONE)
      return error;

    // Load device policy and update stuff that depends on device policy. If
    // there's a config, it means the device is locked and there should also be
    // device policy at this point.
    std::unique_ptr<policy::DevicePolicyImpl> policy_impl =
        std::move(device_policy_impl_for_testing);
    if (!policy_impl)
      policy_impl = std::make_unique<policy::DevicePolicyImpl>();
    if (!policy_impl->LoadPolicy()) {
      LOG(ERROR) << "Failed to load device policy. Authentication and policy "
                    "fetch might behave unexpectedly until the next device "
                    "policy fetch.";
    }

    // Call this even when loading failed to get the defaults right (e.g.
    // turn on machine password auto renewal).
    UpdateDevicePolicyDependencies(policy_impl->get_device_policy());
  }

  return ERROR_NONE;
}

// static
bool SambaInterface::CleanState(const PathService* path_service) {
  // Note: We're not permitted to delete the folder and DeleteFile apparently
  // doesn't support wildcards, so DeleteFile returns false.
  DCHECK(path_service);
  const base::FilePath state_dir(path_service->Get(Path::STATE_DIR));
  base::DeleteFile(state_dir, true /* recursive */);
  if (!base::IsDirectoryEmpty(state_dir)) {
    LOG(ERROR) << "Failed to clean state dir '" << state_dir.value() << "'";
    return false;
  }
  return true;
}

ErrorType SambaInterface::AuthenticateUser(
    const std::string& user_principal_name,
    const std::string& account_id,
    int password_fd,
    ActiveDirectoryAccountInfo* account_info) {
  ReloadDebugFlags();

  ErrorType error = AuthenticateUserInternal(user_principal_name, account_id,
                                             password_fd, account_info);

  last_auth_error_ = error;
  return error;
}

ErrorType SambaInterface::AuthenticateUserInternal(
    const std::string& user_principal_name,
    const std::string& account_id,
    int password_fd,
    ActiveDirectoryAccountInfo* account_info) {
  if (!account_id.empty())
    SetUser(account_id);

  // We technically don't have to be in joined state, but check it anyway,
  // because the device should always be joined during auth.
  if (!IsDeviceJoined())
    return ERROR_NOT_JOINED;

  // Split user_principal_name into parts and normalize.
  std::string user_name, user_realm, normalized_upn;
  if (!ParseUserPrincipalName(user_principal_name, &user_name, &user_realm,
                              &normalized_upn)) {
    return ERROR_PARSE_UPN_FAILED;
  }
  SetUserRealm(user_realm);
  user_tgt_manager_.SetPrincipal(normalized_upn);

  // Acquire Kerberos ticket-granting-ticket for the user account.
  ErrorType error = AcquireUserTgt(password_fd);
  if (error != ERROR_NONE)
    return error;

  // Get account info for the user.
  error = GetAccountInfo(user_name, normalized_upn, account_id, account_info);
  if (error != ERROR_NONE)
    return error;

  // Renew TGT periodically. The usual validity lifetime is 1 day, so this won't
  // happen too often. There's a corner-case if pwdLastSet or userAccountControl
  // are missing, see crbug.com/795758. In that case, GetUserStatus cannot
  // determine the password validity and just *assumes* it's valid. However, the
  // AD admin might have requested the user to change their password. To limit
  // the impact, don't renew the TGT automatically, so that the user will be
  // prompted to relog after 1 day instead of the renewal lifetime of usually 1
  // week.
  bool should_auto_renew = account_info->has_pwd_last_set() &&
                           account_info->has_user_account_control();
  LOG_IF(WARNING, !should_auto_renew)
      << "pwdLastSet or userAccountControl fields missing. Will not "
         "be able to determine password validity. Turning off TGT "
         "renewal to limit lifetime.";
  user_tgt_manager_.EnableTgtAutoRenewal(should_auto_renew);

  if (account_id.empty())
    SetUser(account_info->account_id());

  // Store sAMAccountName for policy fetch. Note that net ads gpo list always
  // wants the sAMAccountName. Also note that pwd_last_set is zero and stale
  // at this point if AcquireTgtWithPassword() set a new password, but that's
  // fine, the timestamp is updated in the next GetUserStatus() call.
  user_account_.user_name = account_info->sam_account_name();
  if (account_info->has_pwd_last_set())
    user_pwd_last_set_ = account_info->pwd_last_set();
  user_logged_in_ = true;
  return ERROR_NONE;
}

ErrorType SambaInterface::GetUserStatus(
    const std::string& user_principal_name,
    const std::string& account_id,
    ActiveDirectoryUserStatus* user_status) {
  ReloadDebugFlags();
  SetUser(account_id);

  // We technically don't have to be in joined state, but check it anyway,
  // because the device should always be joined during getting status.
  if (!IsDeviceJoined())
    return ERROR_NOT_JOINED;

  // Split user_principal_name into parts and normalize.
  std::string user_name, user_realm, normalized_upn;
  if (!ParseUserPrincipalName(user_principal_name, &user_name, &user_realm,
                              &normalized_upn)) {
    return ERROR_PARSE_UPN_FAILED;
  }
  SetUserRealm(user_realm);

  // Determine the status of the TGT.
  ActiveDirectoryUserStatus::TgtStatus tgt_status =
      ActiveDirectoryUserStatus::TGT_VALID;
  ErrorType error = GetUserTgtStatus(&tgt_status);
  if (error != ERROR_NONE)
    return error;

  // If we don't have a valid TGT, we can't GetAccountInfo() because that uses
  // the TGT to authenticate. Thus, just return the TGT status and the last auth
  // error.
  if (tgt_status != ActiveDirectoryUserStatus::TGT_VALID) {
    user_status->set_tgt_status(tgt_status);
    user_status->set_last_auth_error(last_auth_error_);
    return ERROR_NONE;
  }

  // Update smb.conf, IPs, server names etc. for the user account.
  error = UpdateAccountData(&user_account_);
  if (error != ERROR_NONE)
    return error;

  // Get account info for the user.
  ActiveDirectoryAccountInfo account_info;
  error =
      GetAccountInfo("" /* user_name unused */, "" /* normalized_upn unused */,
                     account_id, &account_info);
  if (error != ERROR_NONE)
    return error;

  // Determine the status of the password.
  ActiveDirectoryUserStatus::PasswordStatus password_status =
      GetUserPasswordStatus(account_info);

  *user_status->mutable_account_info() = account_info;
  user_status->set_tgt_status(tgt_status);
  user_status->set_password_status(password_status);
  user_status->set_last_auth_error(last_auth_error_);
  return ERROR_NONE;
}

ErrorType SambaInterface::GetUserKerberosFiles(const std::string& account_id,
                                               KerberosFiles* files) {
  ReloadDebugFlags();
  SetUser(account_id);
  return user_tgt_manager_.GetKerberosFiles(files);
}

ErrorType SambaInterface::JoinMachine(
    const std::string& machine_name,
    const std::string& machine_domain,
    const std::vector<std::string>& machine_ou,
    const std::string& user_principal_name,
    KerberosEncryptionTypes encryption_types,
    int password_fd,
    std::string* joined_domain) {
  ReloadDebugFlags();

  // Prevent joining a second time for security reasons (a hacked Chrome might
  // call this).
  if (IsDeviceJoined())
    return ERROR_ALREADY_JOINED;

  // Split user_principal_name into parts and normalize.
  std::string user_name, user_realm, normalized_upn;
  if (!ParseUserPrincipalName(user_principal_name, &user_name, &user_realm,
                              &normalized_upn)) {
    return ERROR_PARSE_UPN_FAILED;
  }
  AnonymizeRealm(user_realm, kUserRealmPlaceholder);
  anonymizer_->SetReplacement(user_name, kSAMAccountNamePlaceholder);

  std::string join_realm;
  if (!machine_domain.empty()) {
    // Join machine to the given domain (note: realm and domain is the same).
    join_realm = base::ToUpperASCII(machine_domain);
    AnonymizeRealm(join_realm, kDeviceRealmPlaceholder);
  } else {
    // By default, join machine to the user's realm.
    join_realm = user_realm;
  }

  // The netbios name in smb.conf needs to be upper-case, but there is also
  // Samba code that logs the machine name lower-case, so add both here.
  anonymizer_->SetReplacementAllCases(machine_name, kMachineNamePlaceholder);

  // Wipe and (re-)create config. Note that all session data is wiped to make
  // testing easier.
  Reset();
  InitDeviceAccount(base::ToUpperASCII(machine_name), join_realm);

  // Note: Encryption types stay valid through the initial device policy fetch,
  // which, if it succeeds, resets or updates the value.
  SetKerberosEncryptionTypes(encryption_types);

  // Update smb.conf, IPs, server names etc. for the device account.
  ErrorType error = UpdateAccountData(&device_account_);
  if (error != ERROR_NONE) {
    Reset();
    return error;
  }

  // Generate random machine password.
  const std::string machine_pass = GenerateRandomMachinePassword();
  anonymizer_->SetReplacement(machine_pass, kMachinePassPlaceholder);

  // Call net ads join to join the machine to the Active Directory domain.
  ProcessExecutor net_cmd(
      {paths_->Get(Path::NET), "ads", "join", kUserParam, normalized_upn,
       kConfigParam, paths_->Get(Path::DEVICE_SMB_CONF), kDebugParam,
       flags_.net_log_level(), kMachinepassParam + machine_pass});
  if (!machine_ou.empty()) {
    net_cmd.PushArg(kCreatecomputerParam +
                    BuildDistinguishedName(machine_ou, join_realm));
  }
  net_cmd.SetInputFile(password_fd);
  if (!jail_helper_.SetupJailAndRun(&net_cmd, Path::NET_ADS_SECCOMP,
                                    TIMER_NET_ADS_JOIN)) {
    Reset();
    return GetNetError(net_cmd, "join");
  }

  // Store the machine password.
  error = WriteMachinePassword(Path::MACHINE_PASS, machine_pass);
  if (error != ERROR_NONE) {
    Reset();
    return error;
  }

  // Store configuration for subsequent runs of the daemon.
  error = WriteConfiguration();
  if (error != ERROR_NONE) {
    Reset();
    return error;
  }

  // Since we just created the account, set propagation retry to give the
  // password time to propagate through Active Directory.
  device_tgt_manager_.SetPropagationRetry(true);

  // Only if everything worked out, keep the config.
  *joined_domain = join_realm;
  return ERROR_NONE;
}

ErrorType SambaInterface::FetchUserGpos(
    const std::string& account_id, protos::GpoPolicyData* gpo_policy_data) {
  ReloadDebugFlags();
  SetUser(account_id);

  if (!user_logged_in_) {
    LOG(ERROR) << "User not logged in. Please call AuthenticateUser() first.";
    return ERROR_NOT_LOGGED_IN;
  }
  DCHECK(!user_account_.user_name.empty());
  DCHECK(!user_account_.realm.empty());

  // We need user_policy_mode_ to properly fetch user policy, which is read from
  // device policy.
  if (!has_device_policy_) {
    LOG(ERROR)
        << "Unknown user policy mode. Please call FetchDeviceGpos() first.";
    return ERROR_NO_DEVICE_POLICY;
  }

  // Download GPOs for the given user, taking the loopback processing |mode|
  // into account:
  //   USER_POLICY_MODE_DEFAULT: Process user GPOs as usual.
  //   USER_POLICY_MODE_MERGE:   Apply user policy from device GPOs on top of
  //                             user policy from user GPOs.
  //   USER_POLICY_MODE_REPLACE: Only apply user policy from device GPOs.
  ErrorType error;
  std::vector<base::FilePath> gpo_file_paths;
  if (user_policy_mode_ != em::DeviceUserPolicyLoopbackProcessingModeProto::
                               USER_POLICY_MODE_REPLACE) {
    // Update smb.conf, IPs, server names etc for the user account.
    error = UpdateAccountData(&user_account_);
    if (error != ERROR_NONE)
      return error;

    // Download user GPOs with user policy data.
    error = GetGpos(GpoSource::USER, PolicyScope::USER, &gpo_file_paths);
    if (error != ERROR_NONE)
      return error;
  }
  if (user_policy_mode_ != em::DeviceUserPolicyLoopbackProcessingModeProto::
                               USER_POLICY_MODE_DEFAULT) {
    // Acquire Kerberos ticket-granting-ticket for the device account.
    error = AcquireDeviceTgt();
    if (error != ERROR_NONE)
      return error;

    // Download device GPOs with user policy data.
    error = GetGpos(GpoSource::MACHINE, PolicyScope::USER, &gpo_file_paths);
    if (error != ERROR_NONE)
      return error;
  }

  // Parse GPOs and store them in a user+extension policy protobuf.
  std::string gpo_policy_data_blob;
  error = ParseGposIntoProtobuf(gpo_file_paths, kCmdParseUserPreg,
                                &gpo_policy_data_blob);
  if (error != ERROR_NONE)
    return error;

  return ParsePolicyData(gpo_policy_data_blob, gpo_policy_data);
}

ErrorType SambaInterface::FetchDeviceGpos(
    protos::GpoPolicyData* gpo_policy_data) {
  ReloadDebugFlags();

  // Check if the device is domain joined.
  if (!IsDeviceJoined())
    return ERROR_NOT_JOINED;

  // Acquire Kerberos ticket-granting-ticket for the device account.
  ErrorType error = AcquireDeviceTgt();
  if (error != ERROR_NONE)
    return error;

  // Download device GPOs with device policy data.
  std::vector<base::FilePath> gpo_file_paths;
  error = GetGpos(GpoSource::MACHINE, PolicyScope::MACHINE, &gpo_file_paths);
  if (error != ERROR_NONE)
    return error;

  // Parse GPOs and store them in a device+extension policy protobuf.
  std::string gpo_policy_data_blob;
  error = ParseGposIntoProtobuf(gpo_file_paths, kCmdParseDevicePreg,
                                &gpo_policy_data_blob);
  if (error != ERROR_NONE)
    return error;

  error = ParsePolicyData(gpo_policy_data_blob, gpo_policy_data);
  if (error != ERROR_NONE)
    return error;

  // Update stuff that depends on device policy.
  em::ChromeDeviceSettingsProto device_policy;
  if (!device_policy.ParseFromString(
          gpo_policy_data->user_or_device_policy())) {
    LOG(ERROR) << "Failed to parse device policy";
    return ERROR_PARSE_FAILED;
  }
  UpdateDevicePolicyDependencies(device_policy);

  return ERROR_NONE;
}

void SambaInterface::SetDefaultLogLevel(AuthPolicyFlags::DefaultLevel level) {
  flags_default_level_ = level;
  LOG(INFO) << "Flags default level = " << flags_default_level_;
  SaveFlagsDefaultLevel();
}

void SambaInterface::SetDevicePolicyImplForTesting(
    std::unique_ptr<policy::DevicePolicyImpl> policy_impl) {
  device_policy_impl_for_testing = std::move(policy_impl);
}

ErrorType SambaInterface::UpdateKdcIp(AccountData* account) const {
  // Call net ads info to get the KDC IP.
  const std::string& smb_conf_path = paths_->Get(account->smb_conf_path);
  authpolicy::ProcessExecutor net_cmd({paths_->Get(Path::NET), "ads", "info",
                                       kConfigParam, smb_conf_path, kDebugParam,
                                       flags_.net_log_level(), kKerberosParam});
  // Replace a few values immediately in the net_cmd output, see
  // SearchAccountInfo for an explanation.
  anonymizer_->ReplaceSearchArg(kKeyKdcServer, kIpAddressPlaceholder);
  anonymizer_->ReplaceSearchArg(kKeyLdapServer, kIpAddressPlaceholder);
  anonymizer_->ReplaceSearchArg(kKeyLdapServerName, kServerNamePlaceholder);
  const bool net_result = jail_helper_.SetupJailAndRun(
      &net_cmd, Path::NET_ADS_SECCOMP, TIMER_NET_ADS_INFO);
  anonymizer_->ResetSearchArgReplacements();
  if (!net_result)
    return GetNetError(net_cmd, "info");
  const std::string& net_out = net_cmd.GetStdout();

  // Parse the output to find the KDC IP. Enclose in a sandbox for security
  // considerations.
  ProcessExecutor parse_cmd(
      {paths_->Get(Path::PARSER), kCmdParseServerInfo, SerializeFlags(flags_)});
  parse_cmd.SetInputString(net_out);
  if (!jail_helper_.SetupJailAndRun(&parse_cmd, Path::PARSER_SECCOMP,
                                    TIMER_NONE)) {
    // Log net output if it hasn't been done yet.
    net_cmd.LogOutputOnce();
    LOG(ERROR) << "authpolicy_parser parse_server_info failed with exit code "
               << parse_cmd.GetExitCode();
    return ERROR_PARSE_FAILED;
  }

  protos::ServerInfo server_info;
  if (!server_info.ParseFromString(parse_cmd.GetStdout())) {
    // Log net output if it hasn't been done yet.
    net_cmd.LogOutputOnce();
    LOG(ERROR) << "Failed to parse server info protobuf";
    return ERROR_PARSE_FAILED;
  }

  account->kdc_ip = server_info.kdc_ip();
  account->server_time =
      base::Time::FromInternalValue(server_info.server_time());

  // Explicitly set replacements again, see SearchAccountInfo for an
  // explanation.
  anonymizer_->SetReplacementAllCases(account->kdc_ip, kIpAddressPlaceholder);

  return ERROR_NONE;
}

ErrorType SambaInterface::UpdateDcName(AccountData* account) const {
  // Call net ads lookup to get the domain controller name.
  const std::string& smb_conf_path = paths_->Get(account->smb_conf_path);
  authpolicy::ProcessExecutor net_cmd({paths_->Get(Path::NET), "ads", "lookup",
                                       kConfigParam, smb_conf_path, kDebugParam,
                                       flags_.net_log_level(), kKerberosParam});
  // Replace a few values immediately in the net_cmd output, see
  // SearchAccountInfo for an explanation.
  anonymizer_->ReplaceSearchArg(kKeyForest, kForestPlaceholder);
  anonymizer_->ReplaceSearchArg(kKeyDomain, kDomainPlaceholder);
  anonymizer_->ReplaceSearchArg(kKeyDomainController, kServerNamePlaceholder);
  anonymizer_->ReplaceSearchArg(kKeyPreWin2kDomain, kDomainPlaceholder);
  anonymizer_->ReplaceSearchArg(kKeyPreWin2kHostname, kServerNamePlaceholder);
  anonymizer_->ReplaceSearchArg(kKeyServerSiteName, kSiteNamePlaceholder);
  anonymizer_->ReplaceSearchArg(kKeyClientSiteName, kSiteNamePlaceholder);
  const bool net_result = jail_helper_.SetupJailAndRun(
      &net_cmd, Path::NET_ADS_SECCOMP, TIMER_NET_ADS_INFO);
  anonymizer_->ResetSearchArgReplacements();
  if (!net_result)
    return GetNetError(net_cmd, "lookup");
  const std::string& net_out = net_cmd.GetStdout();

  // Parse the output to find the domain controller name. Enclose in a sandbox
  // for security considerations.
  ProcessExecutor parse_cmd(
      {paths_->Get(Path::PARSER), kCmdParseDcName, SerializeFlags(flags_)});
  parse_cmd.SetInputString(net_out);
  if (!jail_helper_.SetupJailAndRun(&parse_cmd, Path::PARSER_SECCOMP,
                                    TIMER_NONE)) {
    // Log net output if it hasn't been done yet.
    net_cmd.LogOutputOnce();
    LOG(ERROR) << "authpolicy_parser parse_dc_name failed with exit code "
               << parse_cmd.GetExitCode();
    return ERROR_PARSE_FAILED;
  }
  account->dc_name = parse_cmd.GetStdout();

  // Explicitly set replacements again, see SearchAccountInfo for an
  // explanation.
  anonymizer_->SetReplacementAllCases(account->dc_name, kServerNamePlaceholder);

  return ERROR_NONE;
}

ErrorType SambaInterface::GetUserTgtStatus(
    ActiveDirectoryUserStatus::TgtStatus* tgt_status) {
  protos::TgtLifetime lifetime;
  ErrorType error = user_tgt_manager_.GetTgtLifetime(&lifetime);
  switch (error) {
    case ERROR_NONE:
      *tgt_status = lifetime.validity_seconds() > 0
                        ? ActiveDirectoryUserStatus::TGT_VALID
                        : ActiveDirectoryUserStatus::TGT_EXPIRED;
      return ERROR_NONE;
    // Eat two errors and convert them to TgtStatus values instead.
    case ERROR_NO_CREDENTIALS_CACHE_FOUND:
      *tgt_status = ActiveDirectoryUserStatus::TGT_NOT_FOUND;
      return ERROR_NONE;
    case ERROR_KERBEROS_TICKET_EXPIRED:
      *tgt_status = ActiveDirectoryUserStatus::TGT_EXPIRED;
      return ERROR_NONE;
    default:
      return error;
  }
}

ActiveDirectoryUserStatus::PasswordStatus SambaInterface::GetUserPasswordStatus(
    const ActiveDirectoryAccountInfo& account_info) {
  // See https://msdn.microsoft.com/en-us/library/ms679430(v=vs.85).aspx.

  // Gracefully handle missing fields, see crbug.com/795758.
  if (!account_info.has_pwd_last_set() ||
      !account_info.has_user_account_control()) {
    return ActiveDirectoryUserStatus::PASSWORD_VALID;
  }

  // Password is always valid if it never expires.
  if ((account_info.user_account_control() & UF_DONT_EXPIRE_PASSWD) != 0)
    return ActiveDirectoryUserStatus::PASSWORD_VALID;

  // Password expired, user will have to enter a new password.
  if (account_info.pwd_last_set() == 0)
    return ActiveDirectoryUserStatus::PASSWORD_EXPIRED;

  // Memorize pwd_last_set if it wasn't set yet. This happens after the password
  // expired and was reset by AuthenticateUser().
  if (user_pwd_last_set_ == 0) {
    user_pwd_last_set_ = account_info.pwd_last_set();
    return ActiveDirectoryUserStatus::PASSWORD_VALID;
  }

  // Password changed on the server. Note: Don't update pwd_last_set_ here,
  // update it in AuthenticateUser() when we know that Chrome sent the right
  // password.
  if (user_pwd_last_set_ != account_info.pwd_last_set())
    return ActiveDirectoryUserStatus::PASSWORD_CHANGED;

  // pwd_last_set did not change, password is still valid.
  return ActiveDirectoryUserStatus::PASSWORD_VALID;
}

ErrorType SambaInterface::UpdateWorkgroup(AccountData* account) {
  const std::string& smb_conf_path = paths_->Get(account->smb_conf_path);
  ProcessExecutor net_cmd({paths_->Get(Path::NET), "ads", "workgroup",
                           kConfigParam, smb_conf_path, kDebugParam,
                           flags_.net_log_level(), kKerberosParam});
  // Parse workgroup from the net_cmd output immediately, see SearchAccountInfo
  // for an explanation. Also replace a bunch of other server names.
  anonymizer_->ReplaceSearchArg(kKeyWorkgroup, kWorkgroupPlaceholder);
  anonymizer_->ReplaceSearchArg(kKeyAdsDnsParseRrSrv, kServerNamePlaceholder,
                                "Parsed (.+?)\\.");
  anonymizer_->ReplaceSearchArg(kKeyPdcDnsName, kServerNamePlaceholder,
                                "'(.+)'");
  anonymizer_->ReplaceSearchArg(kKeyAdsDcName, kServerNamePlaceholder,
                                "using server='(.+?)\\.");
  anonymizer_->ReplaceSearchArg(kKeyPdcName, kServerNamePlaceholder, "'(.+)'");
  anonymizer_->ReplaceSearchArg(kKeyServerSite, kSiteNamePlaceholder, "'(.+)'");
  anonymizer_->ReplaceSearchArg(kKeyClientSite, kSiteNamePlaceholder, "'(.+)'");
  const bool net_result = jail_helper_.SetupJailAndRun(
      &net_cmd, Path::NET_ADS_SECCOMP, TIMER_NET_ADS_WORKGROUP);
  anonymizer_->ResetSearchArgReplacements();
  if (!net_result)
    return GetNetError(net_cmd, "workgroup");
  const std::string& net_out = net_cmd.GetStdout();

  // Parse the output to find the workgroup. Enclose in a sandbox for security
  // considerations.
  ProcessExecutor parse_cmd(
      {paths_->Get(Path::PARSER), kCmdParseWorkgroup, SerializeFlags(flags_)});
  parse_cmd.SetInputString(net_out);
  if (!jail_helper_.SetupJailAndRun(&parse_cmd, Path::PARSER_SECCOMP,
                                    TIMER_NONE)) {
    LOG(ERROR) << "authpolicy_parser parse_workgroup failed with exit code "
               << parse_cmd.GetExitCode();
    return ERROR_PARSE_FAILED;
  }
  account->workgroup = parse_cmd.GetStdout();

  // Explicitly set replacements again, see SearchAccountInfo for an
  // explanation.
  anonymizer_->SetReplacement(account->workgroup, kWorkgroupPlaceholder);
  return ERROR_NONE;
}

ErrorType SambaInterface::WriteSmbConf(const AccountData& account) const {
  // account.netbios_name and account.workgroup may be empty at this point.
  DCHECK(!account.realm.empty());

  std::string data = base::StringPrintf(
      kSmbConfData, account.netbios_name.c_str(), account.workgroup.c_str(),
      account.realm.c_str(), paths_->Get(Path::SAMBA_LOCK_DIR).c_str(),
      paths_->Get(Path::SAMBA_CACHE_DIR).c_str(),
      paths_->Get(Path::SAMBA_STATE_DIR).c_str(),
      paths_->Get(Path::SAMBA_PRIVATE_DIR).c_str(),
      GetEncryptionTypesString(encryption_types_));

  const base::FilePath smbconf_path(paths_->Get(account.smb_conf_path));
  const int data_size = static_cast<int>(data.size());
  if (base::WriteFile(smbconf_path, data.c_str(), data_size) != data_size) {
    LOG(ERROR) << "Failed to write Samba conf file '" << smbconf_path.value()
               << "'";
    return ERROR_LOCAL_IO;
  }

  return ERROR_NONE;
}

ErrorType SambaInterface::UpdateAccountData(AccountData* account) {
  // Write smb.conf for UpdateWorkgroup().
  ErrorType error = WriteSmbConf(*account);
  if (error != ERROR_NONE)
    return error;

  // Update |account|->workgroup.
  const std::string prev_workgroup = account->workgroup;
  error = UpdateWorkgroup(account);
  if (error != ERROR_NONE)
    return error;

  // Write smb.conf again for the rest in case the workgroup changed.
  if (account->workgroup != prev_workgroup) {
    error = WriteSmbConf(*account);
    if (error != ERROR_NONE)
      return error;
  }

  // Query the key distribution center IP and store it in |account|->kdc_ip.
  error = UpdateKdcIp(account);
  if (error != ERROR_NONE)
    return error;

  // Query the domain controller name and store it in |account|->dc_name.
  error = UpdateDcName(account);
  if (error != ERROR_NONE)
    return error;

  return ERROR_NONE;
}

ErrorType SambaInterface::AcquireUserTgt(int password_fd) {
  // Update smb.conf, IPs, server names etc. for the user account.
  ErrorType error = UpdateAccountData(&user_account_);
  if (error != ERROR_NONE)
    return error;
  user_tgt_manager_.SetKdcIp(user_account_.kdc_ip);

  // Call kinit to get the Kerberos ticket-granting-ticket.
  return user_tgt_manager_.AcquireTgtWithPassword(password_fd);
}

ErrorType SambaInterface::AcquireDeviceTgt() {
  // Update smb.conf, IPs, server names etc for the device account.
  ErrorType error = UpdateAccountData(&device_account_);
  if (error != ERROR_NONE)
    return error;
  device_tgt_manager_.SetKdcIp(device_account_.kdc_ip);

  // Acquire the Kerberos ticket-granting-ticket.
  const base::FilePath password_path(paths_->Get(Path::MACHINE_PASS));
  if (!base::PathExists(password_path)) {
    // This is expected to happen on devices that had been domain joined before
    // authpolicyd managed the machine password. They stored the machine keytab
    // instead of the password, so use that for authentication.
    return device_tgt_manager_.AcquireTgtWithKeytab(Path::MACHINE_KEYTAB);
  }

  // Authenticate using password. Note: There is no keytab file here.
  base::ScopedFD password_fd = ReadFileToPipe(password_path);
  if (!password_fd.is_valid()) {
    LOG(ERROR) << "Failed to open machine password file '"
               << password_path.value() << "'";
    return ERROR_LOCAL_IO;
  }
  const base::FilePath prev_password_path(paths_->Get(Path::PREV_MACHINE_PASS));
  error = device_tgt_manager_.AcquireTgtWithPassword(password_fd.get());
  if (error != ERROR_BAD_PASSWORD || !base::PathExists(prev_password_path))
    return error;

  // Try again with the previous password. After a password change the password
  // might not have propagated through a large AD deployment yet.
  password_fd = ReadFileToPipe(prev_password_path);
  if (!password_fd.is_valid()) {
    LOG(ERROR) << "Failed to open machine password file '"
               << prev_password_path.value() << "'";
    return ERROR_LOCAL_IO;
  }
  return device_tgt_manager_.AcquireTgtWithPassword(password_fd.get());
}

ErrorType SambaInterface::WriteMachinePassword(
    Path path, const std::string& machine_pass) const {
  const base::FilePath password_path(paths_->Get(path));
  if (!base::ImportantFileWriter::WriteFileAtomically(password_path,
                                                      machine_pass)) {
    LOG(ERROR) << "Failed to write machine password file '"
               << password_path.value() << "'";
    return ERROR_LOCAL_IO;
  }

  // This file is only authpolicyd's business.
  int mode =
      base::FILE_PERMISSION_READ_BY_USER | base::FILE_PERMISSION_WRITE_BY_USER;
  ErrorType error = SetFilePermissions(password_path, mode);
  if (error != ERROR_NONE)
    return error;

  // Set file time to match server time, so that we can determine the password
  // age and renew the machine password without relying on local time.
  if (!base::TouchFile(password_path, device_account_.server_time,
                       device_account_.server_time)) {
    LOG(ERROR) << "Failed to set file time on machine password file '"
               << password_path.value() << "'";
    return ERROR_LOCAL_IO;
  }

  LOG(INFO) << "Wrote machine password file '" << password_path.value() << "'";
  return ERROR_NONE;
}

ErrorType SambaInterface::RollMachinePassword() {
  const base::FilePath password_path(paths_->Get(Path::MACHINE_PASS));
  const base::FilePath prev_password_path(paths_->Get(Path::PREV_MACHINE_PASS));
  const base::FilePath new_password_path(paths_->Get(Path::NEW_MACHINE_PASS));

  base::File::Error file_error;
  if (!base::ReplaceFile(password_path, prev_password_path, &file_error) ||
      !base::ReplaceFile(new_password_path, password_path, &file_error)) {
    LOG(ERROR) << "Machine password roll failed: "
               << base::File::ErrorToString(file_error);
    return ERROR_LOCAL_IO;
  }

  return ERROR_NONE;
}

ErrorType SambaInterface::WriteConfiguration() const {
  DCHECK(!device_account_.realm.empty());
  DCHECK(!device_account_.netbios_name.empty());

  protos::ActiveDirectoryConfig config;
  config.set_realm(device_account_.realm);
  config.set_machine_name(device_account_.netbios_name);

  std::string config_blob;
  if (!config.SerializeToString(&config_blob)) {
    LOG(ERROR) << "Failed to serialize configuration to string";
    return ERROR_LOCAL_IO;
  }

  const base::FilePath config_path(paths_->Get(Path::CONFIG_DAT));
  const int config_size = static_cast<int>(config_blob.size());
  if (base::WriteFile(config_path, config_blob.c_str(), config_size) !=
      config_size) {
    LOG(ERROR) << "Failed to write configuration file '" << config_path.value()
               << "'";
    return ERROR_LOCAL_IO;
  }

  // This file is only authpolicyd's business.
  ErrorType error =
      SetFilePermissions(config_path, base::FILE_PERMISSION_READ_BY_USER);
  if (error != ERROR_NONE)
    return error;

  LOG(INFO) << "Wrote configuration file '" << config_path.value() << "'";
  return ERROR_NONE;
}

ErrorType SambaInterface::ReadConfiguration() {
  const base::FilePath config_path(paths_->Get(Path::CONFIG_DAT));
  if (!base::PathExists(config_path)) {
    LOG(ERROR) << "Configuration file '" << config_path.value()
               << "' does not exist";
    return ERROR_LOCAL_IO;
  }

  std::string config_blob;
  if (!base::ReadFileToStringWithMaxSize(config_path, &config_blob,
                                         kConfigSizeLimit)) {
    PLOG(ERROR) << "Failed to read configuration file '" << config_path.value()
                << "'";
    return ERROR_LOCAL_IO;
  }

  auto config = std::make_unique<protos::ActiveDirectoryConfig>();
  if (!config->ParseFromString(config_blob)) {
    LOG(ERROR) << "Failed to parse configuration from string";
    return ERROR_LOCAL_IO;
  }

  // Check if the config is valid.
  if (config->machine_name().empty() || config->realm().empty()) {
    LOG(ERROR) << "Configuration is invalid";
    return ERROR_LOCAL_IO;
  }

  InitDeviceAccount(config->machine_name(), config->realm());

  LOG(INFO) << "Read configuration file '" << config_path.value() << "'";

  AnonymizeRealm(device_account_.realm, kDeviceRealmPlaceholder);
  anonymizer_->SetReplacementAllCases(device_account_.netbios_name,
                                      kMachineNamePlaceholder);
  return ERROR_NONE;
}

ErrorType SambaInterface::GetAccountInfo(
    const std::string& user_name,
    const std::string& normalized_upn,
    const std::string& account_id,
    ActiveDirectoryAccountInfo* account_info) {
  // If |account_id| is provided, search by objectGUID only.
  if (!account_id.empty()) {
    // Searching by objectGUID has to use the octet string representation!
    // Note: If |account_id| is malformed, the search yields no results.
    const std::string account_id_octet = GuidToOctetString(account_id);
    anonymizer_->SetReplacement(account_id_octet, kAccountIdPlaceholder);
    std::string search_string =
        base::StringPrintf("(objectGUID=%s)", account_id_octet.c_str());
    return SearchAccountInfo(search_string, account_info);
  }

  // Otherwise, search by sAMAccountName, then by userPrincipalName.
  anonymizer_->SetReplacement(user_name, kSAMAccountNamePlaceholder);
  std::string search_string =
      base::StringPrintf("(sAMAccountName=%s)", user_name.c_str());
  ErrorType error = SearchAccountInfo(search_string, account_info);
  if (error != ERROR_BAD_USER_NAME)  // ERROR_BAD_USER_NAME means there were
    return error;                    // no search results.

  LOG(WARNING) << "Account info not found by sAMAccountName. "
               << "Trying userPrincipalName.";
  anonymizer_->SetReplacement(user_name, kLogonNamePlaceholder);
  search_string =
      base::StringPrintf("(userPrincipalName=%s)", normalized_upn.c_str());
  return SearchAccountInfo(search_string, account_info);
}

ErrorType SambaInterface::SearchAccountInfo(
    const std::string& search_string,
    ActiveDirectoryAccountInfo* account_info) {
  // Set up net ads search to find the user's account info.
  const std::string& smb_conf_path = paths_->Get(user_account_.smb_conf_path);
  ProcessExecutor net_cmd(
      {paths_->Get(Path::NET), "ads", "search", search_string,
       kSearchObjectGUID, kSearchSAMAccountName, kSearchCommonName,
       kSearchDisplayName, kSearchGivenName, kSearchPwdLastSet,
       kSearchUserAccountControl, kConfigParam, smb_conf_path, kDebugParam,
       flags_.net_log_level(), kKerberosParam});

  // Parse the search args from the net_cmd output immediately. This resolves
  // the chicken-egg-problem that replacement strings cannot be set before the
  // strings-to-replace are known, so the output of net_cmd would still contain
  // sensitive strings.
  anonymizer_->ReplaceSearchArg(kSearchObjectGUID, kAccountIdPlaceholder);
  anonymizer_->ReplaceSearchArg(kSearchDisplayName, kDisplayNamePlaceholder);
  anonymizer_->ReplaceSearchArg(kSearchGivenName, kGivenNamePlaceholder);
  anonymizer_->ReplaceSearchArg(kSearchSAMAccountName,
                                kSAMAccountNamePlaceholder);
  anonymizer_->ReplaceSearchArg(kSearchCommonName, kCommonNamePlaceholder);

  // Use the user's TGT to query the account info.
  net_cmd.SetEnv(kKrb5CCEnvKey,
                 paths_->Get(user_tgt_manager_.GetCredentialCachePath()));
  const bool net_result = jail_helper_.SetupJailAndRun(
      &net_cmd, Path::NET_ADS_SECCOMP, TIMER_NET_ADS_SEARCH);
  anonymizer_->ResetSearchArgReplacements();
  if (!net_result) {
    return GetNetError(net_cmd, "search");
  }
  const std::string& net_out = net_cmd.GetStdout();

  // Parse the output to find the account info proto blob. Enclose in a sandbox
  // for security considerations.
  ProcessExecutor parse_cmd({paths_->Get(Path::PARSER), kCmdParseAccountInfo,
                             SerializeFlags(flags_)});
  parse_cmd.SetInputString(net_out);
  if (!jail_helper_.SetupJailAndRun(&parse_cmd, Path::PARSER_SECCOMP,
                                    TIMER_NONE)) {
    // Log net output if it hasn't been done yet.
    net_cmd.LogOutputOnce();
    LOG(ERROR) << "Failed to parse account info. Net response: " << net_out;
    return ERROR_PARSE_FAILED;
  }
  const std::string& account_info_blob = parse_cmd.GetStdout();

  // Parse account info protobuf.
  if (account_info_blob.empty()) {
    // No search results. Return ERROR_BAD_USER_NAME since it usually means that
    // the user mistyped their user name.
    LOG(WARNING) << "Search yielded no results";
    return ERROR_BAD_USER_NAME;
  } else if (!account_info->ParseFromString(account_info_blob)) {
    // Log net output if it hasn't been done yet.
    net_cmd.LogOutputOnce();
    LOG(ERROR) << "Failed to parse account info protobuf";
    return ERROR_PARSE_FAILED;
  }

  // Explicitly set replacements again in case logging is currently disabled
  // and the anonymizer has not parsed the search values above. If we didn't do
  // it here and logging would be enabled later, logs would contain sensitive
  // data.
  anonymizer_->SetReplacement(account_info->account_id(),
                              kAccountIdPlaceholder);
  anonymizer_->SetReplacement(account_info->display_name(),
                              kDisplayNamePlaceholder);
  anonymizer_->SetReplacement(account_info->given_name(),
                              kGivenNamePlaceholder);
  anonymizer_->SetReplacement(account_info->sam_account_name(),
                              kSAMAccountNamePlaceholder);
  anonymizer_->SetReplacement(account_info->common_name(),
                              kCommonNamePlaceholder);

  return ERROR_NONE;
}

ErrorType SambaInterface::GetGpos(GpoSource source,
                                  PolicyScope scope,
                                  std::vector<base::FilePath>* gpo_file_paths) {
  // There's no use case for machine policy from user GPOs right now.
  DCHECK(!(source == GpoSource::USER && scope == PolicyScope::MACHINE));

  // Query list of GPOs from Active Directory server.
  protos::GpoList gpo_list;
  ErrorType error = GetGpoList(source, scope, &gpo_list);
  if (error != ERROR_NONE)
    return error;

  // Download GPOs from Active Directory server.
  return DownloadGpos(gpo_list, source, scope, gpo_file_paths);
}

ErrorType SambaInterface::GetGpoList(GpoSource source,
                                     PolicyScope scope,
                                     protos::GpoList* gpo_list) const {
  DCHECK(gpo_list);
  LOG(INFO) << "Getting " << (scope == PolicyScope::USER ? "user" : "device")
            << " GPO list for "
            << (source == GpoSource::USER ? "user" : "device") << " account";

  const AccountData& account = GetAccount(source);
  const TgtManager& tgt_manager = GetTgtManager(source);
  authpolicy::ProcessExecutor net_cmd(
      {paths_->Get(Path::NET), "ads", "gpo", "list", account.user_name,
       kConfigParam, paths_->Get(account.smb_conf_path), kDebugParam,
       flags_.net_log_level(), kKerberosParam});
  net_cmd.SetEnv(kKrb5CCEnvKey,
                 paths_->Get(tgt_manager.GetCredentialCachePath()));
  if (!jail_helper_.SetupJailAndRun(&net_cmd, Path::NET_ADS_SECCOMP,
                                    TIMER_NET_ADS_GPO_LIST)) {
    return GetNetError(net_cmd, "gpo list");
  }

  // GPO data is written to stderr, not stdin!
  const std::string& net_out = net_cmd.GetStderr();

  // Parse the GPO list. Enclose in a sandbox for security considerations. Note
  // that |cmd| depends on |scope| since the parse command is concerned with the
  // type of policy, not which account a GPO came from.
  const char* cmd = scope == PolicyScope::USER ? kCmdParseUserGpoList
                                               : kCmdParseDeviceGpoList;
  ProcessExecutor parse_cmd(
      {paths_->Get(Path::PARSER), cmd, SerializeFlags(flags_)});
  parse_cmd.SetInputString(net_out);
  if (!jail_helper_.SetupJailAndRun(&parse_cmd, Path::PARSER_SECCOMP,
                                    TIMER_NONE)) {
    // Log net output if it hasn't been done yet.
    net_cmd.LogOutputOnce();
    LOG(ERROR) << "Failed to parse GPO list";
    return ERROR_PARSE_FAILED;
  }
  std::string gpo_list_blob = parse_cmd.GetStdout();

  // Parse GPO list protobuf.
  if (!gpo_list->ParseFromString(gpo_list_blob)) {
    LOG(ERROR) << "Failed to read GPO list protobuf";
    return ERROR_PARSE_FAILED;
  }

  return ERROR_NONE;
}

struct GpoPaths {
  std::string server_;    // GPO file path on server (not a local file path!).
  base::FilePath local_;  // Local GPO file path.
  GpoPaths(const std::string& server, const std::string& local)
      : server_(server), local_(local) {}
};

ErrorType SambaInterface::DownloadGpos(
    const protos::GpoList& gpo_list,
    GpoSource source,
    PolicyScope scope,
    std::vector<base::FilePath>* gpo_file_paths) const {
  metrics_->Report(METRIC_DOWNLOAD_GPO_COUNT, gpo_list.entries_size());
  if (gpo_list.entries_size() == 0) {
    LOG(INFO) << "No GPOs to download";
    return ERROR_NONE;
  }

  // Generate all smb source and linux target directories and create targets.
  ErrorType error;
  std::string smb_command = "prompt OFF;lowercase ON;";
  std::string gpo_share;
  std::vector<GpoPaths> gpo_paths;
  for (int entry_idx = 0; entry_idx < gpo_list.entries_size(); ++entry_idx) {
    const protos::GpoEntry& gpo = gpo_list.entries(entry_idx);

    // Security check, make sure nobody sneaks in smbclient commands.
    if (gpo.share().find(';') != std::string::npos ||
        gpo.directory().find(';') != std::string::npos) {
      LOG(ERROR) << "GPO paths may not contain a ';'";
      return ERROR_BAD_GPOS;
    }

    // All GPOs should have the same share, i.e. come from the same SysVol.
    if (gpo_share.empty()) {
      gpo_share = gpo.share();
    } else if (!base::EqualsCaseInsensitiveASCII(gpo_share, gpo.share())) {
      LOG(ERROR) << "Inconsistent share '" << gpo_share << "' != '"
                 << gpo.share() << "'";
      return ERROR_BAD_GPOS;
    }

    // Figure out local (Linux) and remote (smb) directories.
    const char* preg_dir =
        scope == PolicyScope::USER ? kPRegUserDir : kPRegDeviceDir;
    std::string smb_dir =
        base::StringPrintf("\\%s\\%s", gpo.directory().c_str(), preg_dir);
    std::string linux_dir = paths_->Get(Path::GPO_LOCAL_DIR) + smb_dir;
    std::replace(linux_dir.begin(), linux_dir.end(), '\\', '/');

    // Make local directory.
    const base::FilePath linux_dir_fp(linux_dir);
    error = ::authpolicy::CreateDirectory(linux_dir_fp);
    if (error != ERROR_NONE)
      return error;

    // Set group rwx permissions recursively, so that smbclient can write GPOs
    // there and the parser tool can read the GPOs later.
    error = SetFilePermissionsRecursive(
        linux_dir_fp, base::FilePath(paths_->Get(Path::SAMBA_DIR)),
        kFileMode_rwxrwx);
    if (error != ERROR_NONE)
      return error;

    // Build command for smbclient.
    smb_command += base::StringPrintf("cd %s;lcd %s;get %s;", smb_dir.c_str(),
                                      linux_dir.c_str(), kPRegFileName);

    // Record output file paths.
    gpo_paths.push_back(GpoPaths(smb_dir + "\\" + kPRegFileName,
                                 linux_dir + "/" + kPRegFileName));

    // Delete any preexisting policy file. Otherwise, if downloading the file
    // failed, we wouldn't realize it and use a stale version.
    if (base::PathExists(gpo_paths.back().local_) &&
        !base::DeleteFile(gpo_paths.back().local_, false)) {
      LOG(ERROR) << "Failed to delete old GPO file '"
                 << anonymizer_->Process(gpo_paths.back().local_.value())
                 << "'";
      return ERROR_LOCAL_IO;
    }
  }

  const AccountData& account = GetAccount(source);
  DCHECK(!account.dc_name.empty());
  const std::string service =
      base::StringPrintf("//%s/%s", account.dc_name.c_str(), gpo_share.c_str());

  // The exit code of smbclient corresponds to the LAST command issued. Some
  // files might be missing and fail to download, which is fine and handled
  // below. Appending 'exit' makes sure the exit code is not 1 if the last file
  // happens to be missing.
  smb_command += "exit;";

  // Download GPO into local directory. Retry a couple of times in case of
  // network errors, Kerberos authentication may be flaky in some deployments,
  // see crbug.com/684733.
  ProcessExecutor smb_client_cmd(
      {paths_->Get(Path::SMBCLIENT), service, kConfigParam,
       paths_->Get(account.smb_conf_path), kKerberosParam, kDebugParam,
       flags_.net_log_level(), kCommandParam, smb_command});
  const TgtManager& tgt_manager = GetTgtManager(source);
  smb_client_cmd.SetEnv(kKrb5CCEnvKey,
                        paths_->Get(tgt_manager.GetCredentialCachePath()));
  smb_client_cmd.SetEnv(kKrb5ConfEnvKey,  // Kerberos configuration file path.
                        kFilePrefix + paths_->Get(tgt_manager.GetConfigPath()));
  int tries, failed_tries = 0;
  for (tries = 1; tries <= kSmbClientMaxTries; ++tries) {
    if (tries > 1 && smbclient_retry_sleep_enabled_) {
      base::PlatformThread::Sleep(
          base::TimeDelta::FromSeconds(kSmbClientRetryWaitSeconds));
    }
    if (jail_helper_.SetupJailAndRun(&smb_client_cmd, Path::SMBCLIENT_SECCOMP,
                                     TIMER_SMBCLIENT)) {
      error = ERROR_NONE;
      break;
    }
    failed_tries++;
    error = GetSmbclientError(smb_client_cmd);
    if (error != ERROR_NETWORK_PROBLEM)
      break;
  }
  metrics_->Report(METRIC_SMBCLIENT_FAILED_TRY_COUNT, failed_tries);
  if (error != ERROR_NONE)
    return error;

  // Note that the errors are in stdout and the output is in stderr :-/
  const std::string& smbclient_out_lower =
      base::ToLowerASCII(smb_client_cmd.GetStdout());

  // Make sure the GPO files actually downloaded.
  DCHECK(gpo_file_paths);
  for (const GpoPaths& gpo_path : gpo_paths) {
    if (base::PathExists(gpo_path.local_)) {
      gpo_file_paths->push_back(gpo_path.local_);
    } else {
      // Gracefully handle non-existing GPOs. Testing revealed these cases do
      // exist, see crbug.com/680921.
      const std::string no_file_error_key(
          base::ToLowerASCII(kKeyObjectNameNotFound + gpo_path.server_));
      if (Contains(smbclient_out_lower, no_file_error_key)) {
        LOG_IF(WARNING, flags_.log_gpo())
            << "Ignoring missing preg file '"
            << anonymizer_->Process(gpo_path.local_.value()) << "'";
      } else {
        // Log smbclient output if it hasn't been done yet.
        smb_client_cmd.LogOutputOnce();
        LOG(ERROR) << "Failed to download preg file '"
                   << anonymizer_->Process(gpo_path.local_.value()) << "'";
        return ERROR_SMBCLIENT_FAILED;
      }
    }
  }

  return ERROR_NONE;
}

ErrorType SambaInterface::ParseGposIntoProtobuf(
    const std::vector<base::FilePath>& gpo_file_paths,
    const char* parser_cmd_string,
    std::string* policy_blob) const {
  // Convert file paths to proto blob.
  std::string gpo_file_paths_blob;
  protos::FilePathList fp_proto;
  for (const auto& fp : gpo_file_paths)
    *fp_proto.add_entries() = fp.value();
  if (!fp_proto.SerializeToString(&gpo_file_paths_blob)) {
    LOG(ERROR) << "Failed to serialize policy file paths to protobuf";
    return ERROR_PARSE_PREG_FAILED;
  }

  // Load GPOs into protobuf. Enclose in a sandbox for security considerations.
  ProcessExecutor parse_cmd(
      {paths_->Get(Path::PARSER), parser_cmd_string, SerializeFlags(flags_)});
  parse_cmd.SetInputString(gpo_file_paths_blob);
  if (!jail_helper_.SetupJailAndRun(&parse_cmd, Path::PARSER_SECCOMP,
                                    TIMER_NONE)) {
    LOG(ERROR) << "Failed to parse preg files";
    return ERROR_PARSE_PREG_FAILED;
  }
  *policy_blob = parse_cmd.GetStdout();
  return ERROR_NONE;
}

void SambaInterface::UpdateDevicePolicyDependencies(
    const em::ChromeDeviceSettingsProto& device_policy) {
  has_device_policy_ = true;

  // Get Kerberos encryption types policy. Note that we fall back to strong
  // encryption if the policy is not set.
  KerberosEncryptionTypes enc_types = GetEncryptionTypes(device_policy);
  SetKerberosEncryptionTypes(enc_types);

  // Get loopback processing mode.
  user_policy_mode_ = GetUserPolicyMode(device_policy);

  // Update machine password change rate. Use the default 30 days for now until
  // the DeviceMachinePasswordChangeRate arrives in Chrome OS.
  base::TimeDelta password_change_rate =
      GetMachinePasswordChangeRate(device_policy);
  UpdateMachinePasswordAutoChange(password_change_rate);
}

void SambaInterface::UpdateMachinePasswordAutoChange(
    const base::TimeDelta& rate) {
  password_change_rate_ = rate;

  // Disable password auto change if the rate is non-positive.
  if (password_change_rate_ <= base::TimeDelta::FromDays(0)) {
    password_change_timer_.Stop();
    return;
  }

  // Are we using a machine password at all? Devices joined before the switch
  // from keytab to password still use keytabs, so changing the machine password
  // isn't possible.
  if (!base::PathExists(base::FilePath(paths_->Get(Path::MACHINE_PASS)))) {
    LOG(WARNING)
        << "Cannot change the machine password since this devices still uses "
           "the keytab file. Re-enrolling the device will fix this.";
    return;
  }

  // Start timer for the password change checker.
  if (!password_change_timer_.IsRunning()) {
    base::TimeDelta delta =
        base::TimeDelta::FromMinutes(kPasswordChangeCheckRateMinutes);
    password_change_timer_.Start(
        FROM_HERE, delta, this,
        &SambaInterface::AutoCheckMachinePasswordChange);

    // Perform a check immediately. This usually happens on startup and makes
    // sure we do at least one check during a session.
    AutoCheckMachinePasswordChange();
  }
}

void SambaInterface::AutoCheckMachinePasswordChange() {
  LOG(INFO) << "Running scheduled machine password age check";
  ErrorType error = CheckMachinePasswordChange();
  if (error != ERROR_NONE)
    LOG(ERROR) << "Machine password check failed with error " << error;
  did_password_change_check_run_for_testing_ = true;
  metrics_->ReportError(ERROR_OF_AUTO_MACHINE_PASSWORD_CHANGE, error);
}

ErrorType SambaInterface::CheckMachinePasswordChange() {
  // Get the latest server time.
  UpdateAccountData(&device_account_);

  const base::FilePath password_path(paths_->Get(Path::MACHINE_PASS));
  base::File::Info file_info;
  if (!GetFileInfo(password_path, &file_info)) {
    LOG(ERROR)
        << "Machine password check failed. Could not get info for machine "
        << "password file '" << password_path.value() << "'";
    return ERROR_LOCAL_IO;
  }

  // Check if the password is older than the change rate (=max age).
  base::TimeDelta password_age =
      device_account_.server_time - file_info.last_modified;
  if (password_age < password_change_rate_) {
    int days_left = (password_change_rate_ - password_age).InDays();
    LOG(INFO) << "No need to change machine password (" << days_left
              << " days left)";
    return ERROR_NONE;
  }

  LOG(INFO) << "Machine password is older than "
            << password_change_rate_.InDays() << " days. Changing.";

  // Read the old password.
  std::string old_password;
  if (!base::ReadFileToString(password_path, &old_password)) {
    PLOG(ERROR) << "Could not read machine password file '"
                << password_path.value() << "'";
    return ERROR_LOCAL_IO;
  }

  // Generate and write a new password.
  const std::string new_password = GenerateRandomMachinePassword();
  ErrorType error = WriteMachinePassword(Path::NEW_MACHINE_PASS, new_password);
  if (error != ERROR_NONE)
    return error;

  // Change the machine password on the server.
  error = device_tgt_manager_.ChangePassword(old_password, new_password);
  if (error != ERROR_NONE)
    return error;

  // Roll password files.
  error = RollMachinePassword();

  if (error != ERROR_NONE) {
    // Try writing the new password directly, ignoring the previous one.
    error = WriteMachinePassword(Path::MACHINE_PASS, new_password);
  }

  if (error != ERROR_NONE) {
    // Do a best effort recovering the old password. If that doesn't work, we
    // won't be able to access the machine account anymore!
    ErrorType change_back_error =
        device_tgt_manager_.ChangePassword(new_password, old_password);
    ErrorType write_error =
        WriteMachinePassword(Path::MACHINE_PASS, old_password);
    if (change_back_error != ERROR_NONE || write_error != ERROR_NONE) {
      LOG(ERROR) << "Recovering the old machine password failed. Your device "
                    "is in an invalid state and needs to be re-enrolled.";
    }
    return error;
  }

  LOG(INFO) << "Successfully changed machine password";
  return ERROR_NONE;
}

void SambaInterface::SetUser(const std::string& account_id) {
  // Don't allow authenticating multiple users. Chrome should prevent that.
  CHECK(!account_id.empty());
  CHECK(user_account_id_.empty() || user_account_id_ == account_id)
      << "Multi-user not supported";
  user_account_id_ = account_id;
}

void SambaInterface::SetUserRealm(const std::string& user_realm) {
  // Allow setting the realm only once. This makes sure that nobody calls
  // AuthenticateUser() with a different realm, the call fails and we're stuck
  // with a wrong realm.
  CHECK(!user_realm.empty());
  CHECK(user_account_.realm.empty() || user_account_.realm == user_realm)
      << "Multi-user not supported";
  user_account_.realm = user_realm;
  user_tgt_manager_.SetRealm(user_account_.realm);
  AnonymizeRealm(user_realm, kUserRealmPlaceholder);
}

void SambaInterface::InitDeviceAccount(const std::string& netbios_name,
                                       const std::string& realm) {
  device_account_.netbios_name = netbios_name;
  device_account_.user_name = device_account_.netbios_name + "$";
  device_account_.realm = realm;
  device_tgt_manager_.SetRealm(device_account_.realm);
  device_tgt_manager_.SetPrincipal(device_account_.GetPrincipal());
}

void SambaInterface::SetKerberosEncryptionTypes(
    KerberosEncryptionTypes encryption_types) {
  if (encryption_types_ != encryption_types) {
    LOG(INFO) << "Kerberos encryption types changed to "
              << GetEncryptionTypesString(encryption_types);
  }
  encryption_types_ = encryption_types;
  user_tgt_manager_.SetKerberosEncryptionTypes(encryption_types_);
  device_tgt_manager_.SetKerberosEncryptionTypes(encryption_types_);
}

void SambaInterface::AnonymizeRealm(const std::string& realm,
                                    const char* placeholder) {
  anonymizer_->SetReplacementAllCases(realm, placeholder);

  std::vector<std::string> parts = base::SplitString(
      realm, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (size_t n = 0; n < parts.size(); ++n)
    anonymizer_->SetReplacementAllCases(parts[n], placeholder);
}

bool SambaInterface::IsDeviceJoined() const {
  DCHECK(device_account_.realm.empty() ^ !device_account_.netbios_name.empty());
  return !device_account_.realm.empty() &&
         !device_account_.netbios_name.empty();
}

void SambaInterface::Reset() {
  user_account_id_.clear();
  user_pwd_last_set_ = 0;
  user_logged_in_ = false;
  user_account_ = AccountData(Path::USER_SMB_CONF);
  device_account_ = AccountData(Path::DEVICE_SMB_CONF);
  user_tgt_manager_.Reset();
  device_tgt_manager_.Reset();
  SetKerberosEncryptionTypes(ENC_TYPES_STRONG);
  user_policy_mode_ =
      em::DeviceUserPolicyLoopbackProcessingModeProto::USER_POLICY_MODE_DEFAULT;
  password_change_timer_.Stop();
  password_change_rate_ = base::TimeDelta();
  has_device_policy_ = false;
  device_policy_impl_for_testing.reset();
  did_password_change_check_run_for_testing_ = false;
}

void SambaInterface::LoadFlagsDefaultLevel() {
  const base::FilePath default_level_path(
      paths_->Get(Path::FLAGS_DEFAULT_LEVEL));
  if (!CheckFlagsDefaultLevelValid(default_level_path))
    return;
  std::string level_str;
  if (!base::ReadFileToStringWithMaxSize(default_level_path, &level_str, 16)) {
    PLOG(ERROR) << "Failed to read flags default level from '"
                << default_level_path.value() << "'";
    return;
  }
  int level_int;
  if (!base::StringToInt(level_str, &level_int) ||
      level_int < AuthPolicyFlags::kMinLevel ||
      level_int > AuthPolicyFlags::kMaxLevel) {
    LOG(ERROR) << "Bad flags default level '" << level_str << "'";
    return;
  }
  flags_default_level_ = static_cast<AuthPolicyFlags::DefaultLevel>(level_int);
  LOG(INFO) << "Flags default level = " << flags_default_level_;
}

void SambaInterface::SaveFlagsDefaultLevel() {
  const base::FilePath default_level_path(
      paths_->Get(Path::FLAGS_DEFAULT_LEVEL));
  const std::string level_str = std::to_string(flags_default_level_);
  const int size = static_cast<int>(level_str.size());
  if (flags_default_level_ == AuthPolicyFlags::kQuiet) {
    // Remove the file, kQuiet is the default, anyway.
    if (!base::DeleteFile(default_level_path, false /* recursive */)) {
      PLOG(ERROR) << "Failed to delete flags default level file '"
                  << default_level_path.value() << "'";
    }
  } else {
    // Write the file.
    if (base::WriteFile(default_level_path, level_str.data(), size) != size) {
      PLOG(ERROR) << "Failed to write flags default level to '"
                  << default_level_path.value() << "'";
    }
  }
}

void SambaInterface::ReloadDebugFlags() {
  const base::FilePath default_level_path(
      paths_->Get(Path::FLAGS_DEFAULT_LEVEL));
  if (flags_default_level_ != AuthPolicyFlags::kQuiet &&
      !CheckFlagsDefaultLevelValid(default_level_path)) {
    // Default flags file expired, reset default level.
    flags_default_level_ = AuthPolicyFlags::kQuiet;
  }

  // First set defaults, then load file on top.
  AuthPolicyFlags flags_container;
  flags_container.SetDefaults(flags_default_level_);
  const base::FilePath path(paths_->Get(Path::DEBUG_FLAGS));
  if (flags_container.LoadFromJsonFile(path) ||
      flags_default_level_ != AuthPolicyFlags::kQuiet) {
    flags_container.Dump();
  }
  flags_ = flags_container.Get();

  // Toggle anonymizer.
  anonymizer_->set_disabled(flags_.disable_anonymizer());
}

}  // namespace authpolicy
