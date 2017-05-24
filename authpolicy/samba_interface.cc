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
#include <base/memory/ptr_util.h>
#include <base/single_thread_task_runner.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <base/threading/platform_thread.h>
#include <base/time/time.h>

#include "authpolicy/log_level.h"
#include "authpolicy/platform_helper.h"
#include "authpolicy/process_executor.h"
#include "bindings/authpolicy_containers.pb.h"

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
    "\tkerberos method = secrets and keytab\n"
    "\tkerberos encryption types = strong\n"
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
constexpr std::pair<Path, int> kDirsAndMode[] = {
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

// Flags. Write kFlag* strings to the Path::DEBUG_FLAGS file to toggle flags.
const char kFlagDisableSeccomp[] = "disable_seccomp";
const char kFlagLogSeccomp[] = "log_seccomp";
const char kFlagTraceKinit[] = "trace_kinit";

// Size limit when loading the config file (256 kb).
const size_t kConfigSizeLimit = 256 * 1024;

// Maximum smbclient tries.
const int kSmbClientMaxTries = 5;
// Wait interval between two smbclient tries.
const int kSmbClientRetryWaitSeconds = 1;

// Keys for interpreting net output.
const char kKeyJoinAccessDenied[] = "NT_STATUS_ACCESS_DENIED";
const char kKeyInvalidMachineName[] = "Improperly formed account name";
const char kKeyMachineNameTooLong[] = "Our netbios name can be at most";
const char kKeyUserHitJoinQuota[] =
    "Insufficient quota exists to complete the operation";
const char kKeyJoinFailedToFindDC[] = "failed to find DC";
const char kKeyNoLogonServers[] = "No logon servers";
const char kKeyJoinLogonFailure[] = "Logon failure";

// Keys for interpreting smbclient output.
const char kKeyConnectionReset[] = "NT_STATUS_CONNECTION_RESET";
const char kKeyNetworkTimeout[] = "NT_STATUS_IO_TIMEOUT";
const char kKeyObjectNameNotFound[] =
    "NT_STATUS_OBJECT_NAME_NOT_FOUND opening remote file ";

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

}  // namespace

SambaInterface::SambaInterface(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    AuthPolicyMetrics* metrics,
    const PathService* path_service)
    : metrics_(metrics),
      paths_(path_service),
      jail_helper_(paths_),
      user_tgt_manager_(task_runner,
                        paths_,
                        metrics_,
                        &jail_helper_,
                        Path::USER_KRB5_CONF,
                        Path::USER_CREDENTIAL_CACHE),
      device_tgt_manager_(task_runner,
                          paths_,
                          metrics_,
                          &jail_helper_,
                          Path::DEVICE_KRB5_CONF,
                          Path::DEVICE_CREDENTIAL_CACHE) {
  DCHECK(paths_);
}

ErrorType SambaInterface::Initialize(bool expect_config) {
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
  }

  // Load debug flags from file.
  bool disable_seccomp_filters = false;
  bool log_seccomp_filters = false;
  bool trace_kinit = false;
  std::string flags;
  const std::string& flags_path = paths_->Get(Path::DEBUG_FLAGS);
  if (base::ReadFileToString(base::FilePath(flags_path), &flags)) {
    if (Contains(flags, kFlagDisableSeccomp)) {
      LOG(WARNING) << "Seccomp filters disabled";
      disable_seccomp_filters = true;
    }
    if (Contains(flags, kFlagLogSeccomp)) {
      LOG(WARNING) << "Logging seccomp filter failures";
      log_seccomp_filters = true;
    }
    if (Contains(flags, kFlagTraceKinit)) {
      LOG(WARNING) << "Trace kinit";
      trace_kinit = true;
    }
  }

  // Set debug flags.
  jail_helper_.SetDebugFlags(disable_seccomp_filters, log_seccomp_filters);
  user_tgt_manager_.SetKinitTraceEnabled(trace_kinit);
  device_tgt_manager_.SetKinitTraceEnabled(trace_kinit);

  return ERROR_NONE;
}

// static
bool SambaInterface::CleanState(const PathService* path_service) {
  // Note: We're not permitted to delete the folder and DeleteFile apparently
  // doesn't support wildcards, so DeleteFile returns false.
  DCHECK(path_service);
  base::FilePath state_dir(path_service->Get(Path::STATE_DIR));
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
  // Split user_principal_name into parts and normalize.
  std::string user_name, realm, workgroup, normalized_upn;
  if (!ParseUserPrincipalName(
          user_principal_name, &user_name, &realm, &normalized_upn)) {
    return ERROR_PARSE_UPN_FAILED;
  }

  // Write Samba configuration file.
  ErrorType error = EnsureWorkgroupAndWriteSmbConf();
  if (error != ERROR_NONE)
    return error;

  // Make sure we have realm info.
  protos::RealmInfo realm_info;
  error = GetRealmInfo(&realm_info);
  if (error != ERROR_NONE)
    return error;

  // Get account info for the user.
  error = GetAccountInfo(
      user_name, normalized_upn, account_id, realm_info, account_info);
  if (error != ERROR_NONE)
    return error;

  // Update normalized_upn. This handles the situation when the user name
  // changes on the server and the user logs in with his old user name (e.g.
  // from the pods screen in Chrome).
  normalized_upn = account_info->sam_account_name() + "@" + realm;

  // Call kinit to get the Kerberos ticket-granting-ticket.
  error = user_tgt_manager_.AcquireTgtWithPassword(
      normalized_upn, password_fd, realm, realm_info.kdc_ip());
  if (error != ERROR_NONE)
    return error;

  // Renew TGT periodically. The usual validity lifetime is about 10 hours, so
  // this won't happen too often.
  user_tgt_manager_.EnableTgtAutoRenewal(true);

  // Store sAMAccountName for policy fetch. Note that net ads gpo list always
  // wants the sAMAccountName.
  const std::string account_id_key(kActiveDirectoryPrefix +
                                   account_info->account_id());
  user_id_name_map_[account_id_key] = account_info->sam_account_name();
  return ERROR_NONE;
}

ErrorType SambaInterface::GetUserStatus(
    const std::string& account_id, ActiveDirectoryUserStatus* user_status) {
  // Write Samba configuration file.
  ErrorType error = EnsureWorkgroupAndWriteSmbConf();
  if (error != ERROR_NONE)
    return error;

  // Make sure we have realm info.
  protos::RealmInfo realm_info;
  error = GetRealmInfo(&realm_info);
  if (error != ERROR_NONE)
    return error;

  // Get account info for the user.
  ActiveDirectoryAccountInfo account_info;
  error = GetAccountInfo("" /* user_name unused */,
                         "" /* normalized_upn unused */,
                         account_id,
                         realm_info,
                         &account_info);
  if (error != ERROR_NONE)
    return error;

  // Determine the status of the TGT.
  ActiveDirectoryUserStatus::TgtStatus tgt_status =
      ActiveDirectoryUserStatus::TGT_VALID;
  error = GetUserTgtStatus(&tgt_status);
  if (error != ERROR_NONE)
    return error;

  *user_status->mutable_account_info() = account_info;
  user_status->set_tgt_status(tgt_status);
  return ERROR_NONE;
}

ErrorType SambaInterface::JoinMachine(const std::string& machine_name,
                                      const std::string& user_principal_name,
                                      int password_fd) {
  // Split user principal name into parts.
  std::string user_name, realm, normalized_upn;
  if (!ParseUserPrincipalName(
          user_principal_name, &user_name, &realm, &normalized_upn)) {
    return ERROR_PARSE_UPN_FAILED;
  }

  // Wipe and (re-)create config. Note that all session data is wiped to make
  // testing easier.
  Reset();
  config_ = base::MakeUnique<protos::ActiveDirectoryConfig>();
  config_->set_machine_name(base::ToUpperASCII(machine_name));
  config_->set_realm(realm);

  // Write Samba configuration. Will query the workgroup.
  ErrorType error = EnsureWorkgroupAndWriteSmbConf();
  if (error != ERROR_NONE) {
    Reset();
    return error;
  }

  // Call net ads join to join the machine to the Active Directory domain.
  ProcessExecutor net_cmd({paths_->Get(Path::NET),
                           "ads",
                           "join",
                           "-U",
                           normalized_upn,
                           "-s",
                           paths_->Get(Path::SMB_CONF),
                           "-d",
                           kNetLogLevel});
  net_cmd.SetInputFile(password_fd);
  net_cmd.SetEnv(kKrb5KTEnvKey,  // Machine keytab file path.
                 kFilePrefix + paths_->Get(Path::MACHINE_KT_TEMP));
  if (!jail_helper_.SetupJailAndRun(
          &net_cmd, Path::NET_ADS_SECCOMP, TIMER_NET_ADS_JOIN)) {
    Reset();
    return GetNetError(net_cmd, "join");
  }

  // Prevent that authpolicyd-exec can make changes to the keytab file.
  error = SecureMachineKeyTab();
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

  // Only if everything worked out, keep the config.
  retry_machine_kinit_ = true;
  return ERROR_NONE;
}

ErrorType SambaInterface::FetchUserGpos(const std::string& account_id_key,
                                        std::string* policy_blob) {
  // Get sAMAccountName from account id key (must be logged in to fetch user
  // policy).
  auto iter = user_id_name_map_.find(account_id_key);
  if (iter == user_id_name_map_.end()) {
    LOG(ERROR) << "User not logged in. Please call AuthenticateUser first.";
    return ERROR_NOT_LOGGED_IN;
  }
  const std::string& sam_account_name = iter->second;

  // Write Samba configuration file.
  ErrorType error = EnsureWorkgroupAndWriteSmbConf();
  if (error != ERROR_NONE)
    return error;

  // Make sure we have the domain controller name.
  protos::RealmInfo realm_info;
  error = GetRealmInfo(&realm_info);
  if (error != ERROR_NONE)
    return error;

  // FetchDeviceGpos writes a krb5.conf here. For user policy, there's no need
  // to do that here since we're reusing the TGT generated in AuthenticateUser.

  // Get the list of GPOs for the given user name.
  protos::GpoList gpo_list;
  error = GetGpoList(sam_account_name, PolicyScope::USER, &gpo_list);
  if (error != ERROR_NONE)
    return error;

  // Download GPOs from Active Directory server.
  std::vector<base::FilePath> gpo_file_paths;
  error = DownloadGpos(
      gpo_list, realm_info.dc_name(), PolicyScope::USER, &gpo_file_paths);
  if (error != ERROR_NONE)
    return error;

  // Parse GPOs and store them in a user policy protobuf.
  error = ParseGposIntoProtobuf(gpo_file_paths, kCmdParseUserPreg, policy_blob);
  if (error != ERROR_NONE)
    return error;

  return ERROR_NONE;
}

ErrorType SambaInterface::FetchDeviceGpos(std::string* policy_blob) {
  // Write Samba configuration file.
  ErrorType error = EnsureWorkgroupAndWriteSmbConf();
  if (error != ERROR_NONE)
    return error;

  // Get realm info.
  protos::RealmInfo realm_info;
  error = GetRealmInfo(&realm_info);
  if (error != ERROR_NONE)
    return error;

  // Call kinit to get the Kerberos ticket-granting-ticket. retry_machine_kinit_
  // is true for the first device policy fetch after joining Active Directory,
  // which can be very slow because machine credentials need to propagate
  // through the AD deployment.
  std::string machine_principal =
      config_->machine_name() + "$@" + config_->realm();
  error = device_tgt_manager_.AcquireTgtWithKeytab(machine_principal,
                                                   Path::MACHINE_KT_STATE,
                                                   retry_machine_kinit_,
                                                   config_->realm(),
                                                   realm_info.kdc_ip());
  retry_machine_kinit_ = false;
  if (error != ERROR_NONE)
    return error;

  // Get the list of GPOs for the machine.
  protos::GpoList gpo_list;
  error = GetGpoList(
      config_->machine_name() + "$", PolicyScope::MACHINE, &gpo_list);
  if (error != ERROR_NONE)
    return error;

  // Download GPOs from Active Directory server.
  std::vector<base::FilePath> gpo_file_paths;
  error = DownloadGpos(
      gpo_list, realm_info.dc_name(), PolicyScope::MACHINE, &gpo_file_paths);
  if (error != ERROR_NONE)
    return error;

  // Parse GPOs and store them in a device policy protobuf.
  error =
      ParseGposIntoProtobuf(gpo_file_paths, kCmdParseDevicePreg, policy_blob);
  if (error != ERROR_NONE)
    return error;

  return ERROR_NONE;
}

ErrorType SambaInterface::GetRealmInfo(protos::RealmInfo* realm_info) const {
  authpolicy::ProcessExecutor net_cmd({paths_->Get(Path::NET),
                                       "ads",
                                       "info",
                                       "-s",
                                       paths_->Get(Path::SMB_CONF),
                                       "-d",
                                       kNetLogLevel});
  if (!jail_helper_.SetupJailAndRun(
          &net_cmd, Path::NET_ADS_SECCOMP, TIMER_NET_ADS_INFO)) {
    return GetNetError(net_cmd, "info");
  }
  const std::string& net_out = net_cmd.GetStdout();

  // Parse the output to find the domain controller name. Enclose in a sandbox
  // for security considerations.
  ProcessExecutor parse_cmd({paths_->Get(Path::PARSER), kCmdParseRealmInfo});
  parse_cmd.SetInputString(net_out);
  if (!jail_helper_.SetupJailAndRun(
          &parse_cmd, Path::PARSER_SECCOMP, TIMER_NONE)) {
    LOG(ERROR) << "authpolicy_parser parse_realm_info failed with exit code "
               << parse_cmd.GetExitCode();
    return ERROR_PARSE_FAILED;
  }
  if (!realm_info->ParseFromString(parse_cmd.GetStdout())) {
    LOG(ERROR) << "Failed to parse realm info from string";
    return ERROR_PARSE_FAILED;
  }

  LOG(INFO) << "Found DC name = '" << realm_info->dc_name() << "'";
  LOG(INFO) << "Found KDC IP = '" << realm_info->kdc_ip() << "'";
  return ERROR_NONE;
}

ErrorType SambaInterface::GetUserTgtStatus(
    ActiveDirectoryUserStatus::TgtStatus* tgt_status) {
  protos::TgtLifetime lifetime;
  ErrorType error = user_tgt_manager_.GetTgtLifetime(&lifetime);
  if (error == ERROR_NONE) {
    *tgt_status = lifetime.validity_seconds() > 0
                      ? ActiveDirectoryUserStatus::TGT_VALID
                      : ActiveDirectoryUserStatus::TGT_EXPIRED;
    return ERROR_NONE;
  }

  // Eat two errors and convert them to TgtStatus values instead.
  if (error == ERROR_NO_CREDENTIALS_CACHE_FOUND) {
    *tgt_status = ActiveDirectoryUserStatus::TGT_NOT_FOUND;
    return ERROR_NONE;
  }
  if (error == ERROR_KERBEROS_TICKET_EXPIRED) {
    *tgt_status = ActiveDirectoryUserStatus::TGT_EXPIRED;
    return ERROR_NONE;
  }

  return error;
}

ErrorType SambaInterface::EnsureWorkgroup() {
  if (!workgroup_.empty())
    return ERROR_NONE;

  ProcessExecutor net_cmd({paths_->Get(Path::NET),
                           "ads",
                           "workgroup",
                           "-s",
                           paths_->Get(Path::SMB_CONF),
                           "-d",
                           kNetLogLevel});
  if (!jail_helper_.SetupJailAndRun(
          &net_cmd, Path::NET_ADS_SECCOMP, TIMER_NET_ADS_WORKGROUP)) {
    return GetNetError(net_cmd, "workgroup");
  }
  const std::string& net_out = net_cmd.GetStdout();

  // Parse the output to find the workgroup. Enclose in a sandbox for security
  // considerations.
  ProcessExecutor parse_cmd({paths_->Get(Path::PARSER), kCmdParseWorkgroup});
  parse_cmd.SetInputString(net_out);
  if (!jail_helper_.SetupJailAndRun(
          &parse_cmd, Path::PARSER_SECCOMP, TIMER_NONE)) {
    LOG(ERROR) << "authpolicy_parser parse_workgroup failed with exit code "
               << parse_cmd.GetExitCode();
    return ERROR_PARSE_FAILED;
  }
  workgroup_ = parse_cmd.GetStdout();
  return ERROR_NONE;
}

ErrorType SambaInterface::WriteSmbConf() const {
  if (!config_) {
    LOG(ERROR) << "Missing configuration. Must call JoinMachine first.";
    return ERROR_NOT_JOINED;
  }

  std::string data =
      base::StringPrintf(kSmbConfData,
                         config_->machine_name().c_str(),
                         workgroup_.c_str(),
                         config_->realm().c_str(),
                         paths_->Get(Path::SAMBA_LOCK_DIR).c_str(),
                         paths_->Get(Path::SAMBA_CACHE_DIR).c_str(),
                         paths_->Get(Path::SAMBA_STATE_DIR).c_str(),
                         paths_->Get(Path::SAMBA_PRIVATE_DIR).c_str());

  const base::FilePath smbconf_path(paths_->Get(Path::SMB_CONF));
  const int data_size = static_cast<int>(data.size());
  if (base::WriteFile(smbconf_path, data.c_str(), data_size) != data_size) {
    LOG(ERROR) << "Failed to write Samba conf file '" << smbconf_path.value()
               << "'";
    return ERROR_LOCAL_IO;
  }

  return ERROR_NONE;
}

ErrorType SambaInterface::EnsureWorkgroupAndWriteSmbConf() {
  if (workgroup_.empty()) {
    // EnsureWorkgroup requires an smb.conf file, write one with empty
    // workgroup.
    ErrorType error = WriteSmbConf();
    if (error != ERROR_NONE)
      return error;

    error = EnsureWorkgroup();
    if (error != ERROR_NONE)
      return error;
  }

  // Write smb.conf (potentially again, with valid workgroup).
  return WriteSmbConf();
}

ErrorType SambaInterface::WriteConfiguration() const {
  DCHECK(config_);
  std::string config_blob;
  if (!config_->SerializeToString(&config_blob)) {
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
  if (!base::ReadFileToStringWithMaxSize(
          config_path, &config_blob, kConfigSizeLimit)) {
    LOG(ERROR) << "Failed to read configuration file '" << config_path.value()
               << "'";
    return ERROR_LOCAL_IO;
  }

  auto config = base::MakeUnique<protos::ActiveDirectoryConfig>();
  if (!config->ParseFromString(config_blob)) {
    LOG(ERROR) << "Failed to parse configuration from string";
    return ERROR_LOCAL_IO;
  }

  // Check if the config is valid.
  if (config->machine_name().empty() || config->realm().empty()) {
    LOG(ERROR) << "Configuration is invalid";
    return ERROR_LOCAL_IO;
  }

  config_ = std::move(config);
  LOG(INFO) << "Read configuration file '" << config_path.value() << "'";
  return ERROR_NONE;
}

ErrorType SambaInterface::SecureMachineKeyTab() const {
  // At this point, tmp_kt_fp is rw for authpolicyd-exec only, so we, i.e.
  // user authpolicyd, cannot read it. Thus, change file permissions as
  // authpolicyd-exec user, so that the authpolicyd group can read it.
  const base::FilePath temp_kt_fp(paths_->Get(Path::MACHINE_KT_TEMP));
  const base::FilePath state_kt_fp(paths_->Get(Path::MACHINE_KT_STATE));
  ErrorType error;

  // Set group read permissions on keytab as authpolicyd-exec, so we can copy it
  // as authpolicyd (and own the copy).
  {
    ScopedSwitchToSavedUid switch_scope;
    error = SetFilePermissions(temp_kt_fp, kFileMode_rwr);
    if (error != ERROR_NONE)
      return error;
  }

  // Create empty file in destination directory. Note that it is created with
  // rw_r__r__ permissions.
  if (base::WriteFile(state_kt_fp, nullptr, 0) != 0) {
    LOG(ERROR) << "Failed to create file '" << state_kt_fp.value() << "'";
    return ERROR_LOCAL_IO;
  }

  // Revoke 'read by others' permission. We could also just copy temp_kt_fp to
  // state_kt_fp (see below) and revoke the read permission afterwards, but then
  // state_kt_fp would be readable by anyone for a split second, causing a
  // potential security risk.
  error = SetFilePermissions(state_kt_fp, kFileMode_rwr);
  if (error != ERROR_NONE)
    return error;

  // Now we may copy the file. The copy is owned by authpolicyd:authpolicyd.
  if (!base::CopyFile(temp_kt_fp, state_kt_fp)) {
    PLOG(ERROR) << "Failed to copy file '" << temp_kt_fp.value() << "' to '"
                << state_kt_fp.value() << "'";
    return ERROR_LOCAL_IO;
  }

  // Clean up temp file (must be done as authpolicyd-exec).
  {
    ScopedSwitchToSavedUid switch_scope;
    if (!base::DeleteFile(temp_kt_fp, false)) {
      LOG(ERROR) << "Failed to delete file '" << temp_kt_fp.value() << "'";
      return ERROR_LOCAL_IO;
    }
  }

  return ERROR_NONE;
}

ErrorType SambaInterface::GetAccountInfo(
    const std::string& user_name,
    const std::string& normalized_upn,
    const std::string& account_id,
    const protos::RealmInfo& realm_info,
    ActiveDirectoryAccountInfo* account_info) {
  // Refresh the device TGT. Note that the user TGT might not be accessible at
  // this point (we need the sAMAccountName returned in |account_info| to fetch
  // the user TGT).
  std::string machine_principal =
      config_->machine_name() + "$@" + config_->realm();
  ErrorType error =
      device_tgt_manager_.AcquireTgtWithKeytab(machine_principal,
                                               Path::MACHINE_KT_STATE,
                                               false,
                                               config_->realm(),
                                               realm_info.kdc_ip());
  if (error != ERROR_NONE)
    return error;

  // If |account_id| is provided, search by objectGUID only.
  if (!account_id.empty()) {
    // Searching by objectGUID has to use the octet string representation!
    // Note: If |account_id| is malformed, the search yields no results.
    const std::string account_id_octet = GuidToOctetString(account_id);
    std::string search_string =
        base::StringPrintf("(objectGUID=%s)", account_id_octet.c_str());
    return SearchAccountInfo(search_string, account_info);
  }

  // Otherwise, search by sAMAccountName, then by userPrincipalName.
  std::string search_string =
      base::StringPrintf("(sAMAccountName=%s)", user_name.c_str());
  error = SearchAccountInfo(search_string, account_info);
  if (error != ERROR_BAD_USER_NAME)  // ERROR_BAD_USER_NAME means there were
    return error;                    // no search results.

  LOG(WARNING) << "Account info not found by sAMAccountName. "
               << "Trying userPrincipalName.";
  search_string =
      base::StringPrintf("(userPrincipalName=%s)", normalized_upn.c_str());
  return SearchAccountInfo(search_string, account_info);
}

ErrorType SambaInterface::SearchAccountInfo(
    const std::string& search_string,
    ActiveDirectoryAccountInfo* account_info) {
  // Call net ads search to find the user's account info.
  ProcessExecutor net_cmd({paths_->Get(Path::NET),
                           "ads",
                           "search",
                           search_string,
                           kSearchObjectGUID,
                           kSearchSAMAccountName,
                           kSearchDisplayName,
                           kSearchGivenName,
                           "-s",
                           paths_->Get(Path::SMB_CONF),
                           "-d",
                           kNetLogLevel});
  // Use the machine TGT to query the account info.
  net_cmd.SetEnv(kKrb5CCEnvKey,
                 paths_->Get(device_tgt_manager_.GetCredentialCachePath()));
  if (!jail_helper_.SetupJailAndRun(
          &net_cmd, Path::NET_ADS_SECCOMP, TIMER_NET_ADS_SEARCH)) {
    return GetNetError(net_cmd, "search");
  }
  const std::string& net_out = net_cmd.GetStdout();

  // Parse the output to find the account info proto blob. Enclose in a sandbox
  // for security considerations.
  ProcessExecutor parse_cmd({paths_->Get(Path::PARSER), kCmdParseAccountInfo});
  parse_cmd.SetInputString(net_out);
  if (!jail_helper_.SetupJailAndRun(
          &parse_cmd, Path::PARSER_SECCOMP, TIMER_NONE)) {
    LOG(ERROR) << "Failed to get user account id. Net response: " << net_out;
    return ERROR_PARSE_FAILED;
  }
  const std::string& account_info_blob = parse_cmd.GetStdout();

  // Parse account info protobuf.
  if (account_info_blob.empty()) {
    // No search results. Return ERROR_BAD_USER_NAME since it usually means that
    // the user mistyped his user name.
    LOG(WARNING) << "Search yielded no results";
    return ERROR_BAD_USER_NAME;
  } else if (!account_info->ParseFromString(account_info_blob)) {
    LOG(ERROR) << "Failed to parse account info protobuf";
    return ERROR_PARSE_FAILED;
  }

  return ERROR_NONE;
}

ErrorType SambaInterface::GetGpoList(const std::string& user_or_machine_name,
                                     PolicyScope scope,
                                     protos::GpoList* gpo_list) const {
  DCHECK(gpo_list);
  LOG(INFO) << "Getting GPO list for " << user_or_machine_name;

  // Machine names are names ending with $, anything else is a user name.
  authpolicy::ProcessExecutor net_cmd({paths_->Get(Path::NET),
                                       "ads",
                                       "gpo",
                                       "list",
                                       user_or_machine_name,
                                       "-s",
                                       paths_->Get(Path::SMB_CONF),
                                       "-d",
                                       kNetLogLevel});
  const TgtManager& tgt_manager =
      scope == PolicyScope::USER ? user_tgt_manager_ : device_tgt_manager_;
  net_cmd.SetEnv(kKrb5CCEnvKey,
                 paths_->Get(tgt_manager.GetCredentialCachePath()));
  if (!jail_helper_.SetupJailAndRun(
          &net_cmd, Path::NET_ADS_SECCOMP, TIMER_NET_ADS_GPO_LIST)) {
    return GetNetError(net_cmd, "gpo list");
  }

  // GPO data is written to stderr, not stdin!
  const std::string& net_out = net_cmd.GetStderr();

  // Parse the GPO list. Enclose in a sandbox for security considerations.
  const char* cmd = scope == PolicyScope::USER ? kCmdParseUserGpoList
                                               : kCmdParseDeviceGpoList;
  ProcessExecutor parse_cmd({paths_->Get(Path::PARSER), cmd});
  parse_cmd.SetInputString(net_out);
  if (!jail_helper_.SetupJailAndRun(
          &parse_cmd, Path::PARSER_SECCOMP, TIMER_NONE)) {
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
    const std::string& domain_controller_name,
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
  std::string gpo_basepath;
  std::vector<GpoPaths> gpo_paths;
  for (int entry_idx = 0; entry_idx < gpo_list.entries_size(); ++entry_idx) {
    const protos::GpoEntry& gpo = gpo_list.entries(entry_idx);

    // Security check, make sure nobody sneaks in smbclient commands.
    if (gpo.basepath().find(';') != std::string::npos ||
        gpo.directory().find(';') != std::string::npos) {
      LOG(ERROR) << "GPO paths may not contain a ';'";
      return ERROR_BAD_GPOS;
    }

    // All GPOs should have the same basepath, i.e. come from the same SysVol.
    if (gpo_basepath.empty()) {
      gpo_basepath = gpo.basepath();
    } else if (!base::EqualsCaseInsensitiveASCII(gpo_basepath,
                                                 gpo.basepath())) {
      LOG(ERROR) << "Inconsistent base path '" << gpo_basepath << "' != '"
                 << gpo.basepath() << "'";
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
        linux_dir_fp,
        base::FilePath(paths_->Get(Path::SAMBA_DIR)),
        kFileMode_rwxrwx);
    if (error != ERROR_NONE)
      return error;

    // Build command for smbclient.
    smb_command += base::StringPrintf("cd %s;lcd %s;get %s;",
                                      smb_dir.c_str(),
                                      linux_dir.c_str(),
                                      kPRegFileName);

    // Record output file paths.
    gpo_paths.push_back(GpoPaths(smb_dir + "\\" + kPRegFileName,
                                 linux_dir + "/" + kPRegFileName));

    // Delete any preexisting policy file. Otherwise, if downloading the file
    // failed, we wouldn't realize it and use a stale version.
    if (base::PathExists(gpo_paths.back().local_) &&
        !base::DeleteFile(gpo_paths.back().local_, false)) {
      LOG(ERROR) << "Failed to delete old GPO file '"
                 << gpo_paths.back().local_.value().c_str() << "'";
      return ERROR_LOCAL_IO;
    }
  }

  const std::string service = base::StringPrintf(
      "//%s.%s", domain_controller_name.c_str(), gpo_basepath.c_str());

  // Download GPO into local directory. Retry a couple of times in case of
  // network errors, Kerberos authentication may be flaky in some deployments,
  // see crbug.com/684733.
  ProcessExecutor smb_client_cmd({paths_->Get(Path::SMBCLIENT),
                                  service,
                                  "-s",
                                  paths_->Get(Path::SMB_CONF),
                                  "-k",
                                  "-c",
                                  smb_command});
  const TgtManager& tgt_manager =
      scope == PolicyScope::USER ? user_tgt_manager_ : device_tgt_manager_;
  smb_client_cmd.SetEnv(kKrb5CCEnvKey,
                        paths_->Get(tgt_manager.GetCredentialCachePath()));
  smb_client_cmd.SetEnv(kKrb5ConfEnvKey,  // Kerberos configuration file path.
                        kFilePrefix + paths_->Get(tgt_manager.GetConfigPath()));
  bool success = false;
  int tries, failed_tries = 0;
  for (tries = 1; tries <= kSmbClientMaxTries; ++tries) {
    if (tries > 1 && smbclient_retry_sleep_enabled_) {
      base::PlatformThread::Sleep(
          base::TimeDelta::FromSeconds(kSmbClientRetryWaitSeconds));
    }
    success = jail_helper_.SetupJailAndRun(
        &smb_client_cmd, Path::SMBCLIENT_SECCOMP, TIMER_SMBCLIENT);
    if (success) {
      error = ERROR_NONE;
      break;
    }
    failed_tries++;
    error = GetSmbclientError(smb_client_cmd);
    if (error != ERROR_NETWORK_PROBLEM)
      break;
  }
  metrics_->Report(METRIC_SMBCLIENT_FAILED_TRY_COUNT, failed_tries);

  if (!success) {
    // The exit code of smbclient corresponds to the LAST command issued. Thus,
    // Execute() might fail if the last GPO file is missing, which creates an
    // ERROR_SMBCLIENT_FAILED error code. However, we handle this below (not an
    // error), so only error out here on other error codes.
    if (error != ERROR_SMBCLIENT_FAILED)
      return error;
    error = ERROR_NONE;
  }

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
        LOG(WARNING) << "Ignoring missing preg file '"
                     << gpo_path.local_.value() << "'";
      } else {
        LOG(ERROR) << "Failed to download preg file '"
                   << gpo_path.local_.value() << "'";
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
  ProcessExecutor parse_cmd({paths_->Get(Path::PARSER), parser_cmd_string});
  parse_cmd.SetInputString(gpo_file_paths_blob);
  if (!jail_helper_.SetupJailAndRun(
          &parse_cmd, Path::PARSER_SECCOMP, TIMER_NONE)) {
    LOG(ERROR) << "Failed to parse preg files";
    return ERROR_PARSE_PREG_FAILED;
  }
  *policy_blob = parse_cmd.GetStdout();
  return ERROR_NONE;
}

void SambaInterface::Reset() {
  user_id_name_map_.clear();
  config_.reset();
  workgroup_.clear();
  retry_machine_kinit_ = false;
}

}  // namespace authpolicy
