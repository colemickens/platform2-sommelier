// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "authpolicy/samba_interface.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <base/files/file.h>
#include <base/files/file_util.h>
#include <base/memory/ptr_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <base/threading/platform_thread.h>
#include <base/time/time.h>

#include "authpolicy/constants.h"
#include "authpolicy/process_executor.h"
#include "authpolicy/samba_interface_internal.h"
#include "bindings/authpolicy_containers.pb.h"

namespace ac = authpolicy::constants;
namespace ai = authpolicy::internal;
namespace ap = authpolicy::protos;

namespace authpolicy {
namespace {

// Must match Chromium AccountId::kKeyAdIdPrefix.
const char kActiveDirectoryPrefix[] = "a-";

// Temporary data. Note: Use a #define, so we can concat strings below.
#define AUTHPOLICY_TMP_DIR "/tmp/authpolicyd"

// Persisted samba data.
#define STATE_DIR "/var/lib/authpolicyd"

// Temporary samba data.
#define SAMBA_TMP_DIR AUTHPOLICY_TMP_DIR "/samba"

// Kerberos configuration file.
#define KRB5_FILE_PATH AUTHPOLICY_TMP_DIR "/krb5.conf"

// Temp machine keytab file.
#define MACHINE_KT_TMP_FILE_PATH SAMBA_TMP_DIR "/krb5_machine.keytab"

// Persistent machine keytab file.
#define MACHINE_KT_STATE_FILE_PATH STATE_DIR "/krb5_machine.keytab"

// Samba configuration file data.
const char kSmbConfData[] =
    "[global]\n"
    "\tnetbios name = %s\n"
    "\tsecurity = ADS\n"
    "\tworkgroup = %s\n"
    "\trealm = %s\n"
    "\tlock directory = " SAMBA_TMP_DIR "/lock\n"
    "\tcache directory = " SAMBA_TMP_DIR "/cache\n"
    "\tstate directory = " SAMBA_TMP_DIR "/state\n"
    "\tprivate directory = " SAMBA_TMP_DIR "/private\n"
    "\tkerberos method = secrets and keytab\n"
    "\tkerberos encryption types = strong\n"
    "\tclient signing = mandatory\n"
    "\tclient min protocol = SMB2\n"
    // TODO(ljusten): Remove this line once crbug.com/662440 is resolved.
    "\tclient max protocol = SMB3\n"
    "\tclient ipc min protocol = SMB2\n"
    "\tclient schannel = yes\n"
    "\tclient ldap sasl wrapping = sign\n";

// Kerberos configuration file data.
const char kKrb5ConfData[] =
    "[libdefaults]\n"
    // Only allow AES. (No DES, no RC4.)
    "\tdefault_tgs_enctypes = aes256-cts-hmac-sha1-96 aes128-cts-hmac-sha1-96\n"
    "\tdefault_tkt_enctypes = aes256-cts-hmac-sha1-96 aes128-cts-hmac-sha1-96\n"
    "\tpermitted_enctypes = aes256-cts-hmac-sha1-96 aes128-cts-hmac-sha1-96\n"
    // Prune weak ciphers from the above list. With current settings it’s a
    // no-op, but still.
    "\tallow_weak_crypto = false\n"
    // Default is 300 seconds, but we might add a policy for that in the future.
    "\tclockskew = 300\n"
    // Required for password change.
    "\tdefault_realm = %s\n";

const int kFileMode_rwr = base::FILE_PERMISSION_READ_BY_USER |
                          base::FILE_PERMISSION_WRITE_BY_USER |
                          base::FILE_PERMISSION_READ_BY_GROUP;

const int kFileMode_rwxrx = kFileMode_rwr |
                            base::FILE_PERMISSION_EXECUTE_BY_USER |
                            base::FILE_PERMISSION_EXECUTE_BY_GROUP;

const int kFileMode_rwxrwx =
    kFileMode_rwxrx | base::FILE_PERMISSION_WRITE_BY_GROUP;

// Directories to create (Samba fails to do it on its own). AUTHPOLICY_TMP_DIR
// needs group rx access to read smb.conf and krb5.conf and to access
// SAMBA_TMP_DIR, but no write access. SAMBA_TMP_DIR needs full group rwx access
// since samba reads and writes files there.
std::pair<const char*, int> kSambaDirsAndMode[] = {
    {AUTHPOLICY_TMP_DIR,       kFileMode_rwxrx },
    {SAMBA_TMP_DIR,            kFileMode_rwxrwx},
    {SAMBA_TMP_DIR "/lock",    kFileMode_rwxrwx},
    {SAMBA_TMP_DIR "/cache",   kFileMode_rwxrwx},
    {SAMBA_TMP_DIR "/state",   kFileMode_rwxrwx},
    {SAMBA_TMP_DIR "/private", kFileMode_rwxrwx}};

// Location to download GPOs to.
const char kSambaTmpDir[] = SAMBA_TMP_DIR;
const char kGpoLocalDir[] = SAMBA_TMP_DIR "/cache/gpo_cache";

// Directory / filenames for user and device policy.
const char kPRegUserDir[] = "User";
const char kPRegDeviceDir[] = "Machine";
const char kPRegFileName[] = "Registry.pol";

// File paths.
const char kSmbFilePath[] = AUTHPOLICY_TMP_DIR "/smb.conf";
const char kKrb5FilePath[] = KRB5_FILE_PATH;
const char kMachineKtTmpFilePath[] = MACHINE_KT_TMP_FILE_PATH;
const char kMachineKtStateFilePath[] = MACHINE_KT_STATE_FILE_PATH;
const char kConfigFilePath[] = STATE_DIR "/config.dat";

// Flags. Write kFlag* strings to kFlagsFilePath to toggle flags.
const char kFlagsFilePath[] = "/etc/authpolicyd_flags";
const char kFlagDisableSeccomp[] = "disable_seccomp";
const char kFlagLogSeccomp[] = "log_seccomp";
static bool s_disable_seccomp_filters = false;
static bool s_log_seccomp_filters = false;

// Size limit when loading the config file (256 kb).
const size_t kConfigSizeLimit = 256 * 1024;

// Env variable for krb5.conf file.
const char kKrb5ConfEnvKey[] = "KRB5_CONFIG";
const char kKrb5ConfEnvValue[] = "FILE:" KRB5_FILE_PATH;

// Env variable for machine keytab (machine password for getting device policy).
const char kMachineKTEnvKey[] = "KRB5_KTNAME";
const char kMachineKTEnvValueTmp[] = "FILE:" MACHINE_KT_TMP_FILE_PATH;
const char kMachineKTEnvValueState[] = "FILE:" MACHINE_KT_STATE_FILE_PATH;

// Executable paths. For security reasons use absolute file paths!
const char kNetPath[] = "/usr/bin/net";
const char kKInitPath[] = "/usr/bin/kinit";
const char kSmbClientPath[] = "/usr/bin/smbclient";

// Maximum kinit retries for a recently created machine account.
const int kMachineKinitMaxRetries = 60;
// Wait interval between two kinit retries.
const int kMachineKinitRetryWaitSeconds = 1;

// Keys for interpreting kinit output.
const char kKeyBadUserName[] =
    "not found in Kerberos database while getting initial credentials";
const char kKeyBadPassword[] =
    "Preauthentication failed while getting initial credentials";
const char kKeyPasswordExpiredStdout[] =
    "Password expired.  You must change it now.";
const char kKeyPasswordExpiredStderr[] =
    "Cannot read password while getting initial credentials";
const char kKeyCannotResolve[] =
    "Cannot resolve network address for KDC in realm";

// Keys for interpreting net output.
const char kKeyJoinAccessDenied[] = "NT_STATUS_ACCESS_DENIED";
const char kKeyBadMachineName[] = "Improperly formed account name";
const char kKeyMachineNameTooLong[] = "Our netbios name can be at most";
const char kKeyUserHitJoinQuota[] =
    "Insufficient quota exists to complete the operation";
const char kKeyJoinFailedToFindDC[] = "failed to find DC";
const char kKeyNoLogonServers[] = "No logon servers";
const char kKeyJoinLogonFailure[] = "Logon failure";

// Keys for interpreting smbclient output.
const char kKeyNetworkTimeout[] = "NT_STATUS_IO_TIMEOUT";
const char kKeyNoSuchFile[] = "NT_STATUS_NO_SUCH_FILE listing ";

#undef MACHINE_KT_STATE_FILE_PATH
#undef MACHINE_KT_TMP_FILE_PATH
#undef KRB5_FILE_PATH
#undef SAMBA_TMP_DIR
#undef STATE_DIR
#undef AUTHPOLICY_TMP_DIR

// Seccomp filters.
#define SECCOMP_DIR "/usr/share/policy/"
const char kParserSeccompFilter[] =
    SECCOMP_DIR "authpolicy_parser-seccomp.policy";
const char kKInitSeccompFilter[] = SECCOMP_DIR "kinit-seccomp.policy";
const char kNetAdsSeccompFilter[] =
    SECCOMP_DIR "net_ads-seccomp.policy";
const char kSmbClientSeccompFilter[] = SECCOMP_DIR "smbclient-seccomp.policy";
#undef SECCOMP_DIR

// Switches to the authpolicyd-exec user in the constructor and back to
// authpolicyd in the destructor.
class ScopedAuthpolicyExecSwitch {
 public:
  ScopedAuthpolicyExecSwitch() {
    // Keep kAuthPolicydUid as saved uid, so we can switch back.
    CHECK_EQ(0, setresuid(ac::kAuthPolicyExecUid, ac::kAuthPolicyExecUid,
                          ac::kAuthPolicydUid));
  }

  ~ScopedAuthpolicyExecSwitch() {
    // Keep kAuthPolicyExecUid as saved uid, so we can switch back.
    CHECK_EQ(0, setresuid(ac::kAuthPolicydUid, ac::kAuthPolicydUid,
                          ac::kAuthPolicyExecUid));
  }
};

bool SetupJailAndRun(ProcessExecutor* cmd, const char* seccomp_filter) {
  // Limit the system calls that the process can do.
  DCHECK(cmd);
  if (!s_disable_seccomp_filters) {
    if (s_log_seccomp_filters)
      cmd->LogSeccompFilterFailures();
    cmd->SetSeccompFilter(seccomp_filter);
  }

  // Required since we don't have the caps to wipe supplementary groups.
  cmd->KeepSupplementaryGroups();

  // Allows us to drop setgroups, setresgid and setresuid from seccomp filters.
  cmd->SetNoNewPrivs();

  // Execute as authpolicyd exec user. Don't use minijail to switch user. This
  // would force us to run without preload library since saved uids are wiped by
  // execve and the executed code wouldn't be able to switch user. Running with
  // preload library has two main advantages:
  //   1) Tighter seccomp filters, no need to allow execve and others.
  //   2) Ability to log seccomp filter failures. Without this, it is hard to
  //      know which syscall has to be added to the filter policy file.
  ScopedAuthpolicyExecSwitch switch_scope;
  return cmd->Execute();
}

// Returns true if the string contains the given substring.
bool Contains(const std::string& str, const std::string& substr) {
  return str.find(substr) != std::string::npos;
}

ErrorType GetKinitError(const ProcessExecutor& kinit_cmd) {
  // Handle different error cases
  const std::string& kinit_out = kinit_cmd.GetStdout();
  const std::string& kinit_err = kinit_cmd.GetStderr();

  if (Contains(kinit_err, kKeyBadUserName)) {
    LOG(ERROR) << "kinit failed - bad user name";
    return ERROR_BAD_USER_NAME;
  }
  if (Contains(kinit_err, kKeyBadPassword)) {
    LOG(ERROR) << "kinit failed - bad password";
    return ERROR_BAD_PASSWORD;
  }
  // Check both stderr and stdout here since any kinit error in the change-
  // password-workflow would otherwise be interpreted as 'password expired'.
  if (Contains(kinit_out, kKeyPasswordExpiredStdout) &&
      Contains(kinit_err, kKeyPasswordExpiredStderr)) {
    LOG(ERROR) << "kinit failed - password expired";
    return ERROR_PASSWORD_EXPIRED;
  }
  if (Contains(kinit_err, kKeyCannotResolve)) {
    LOG(ERROR) << "kinit failed - cannot resolve KDC realm";
    return ERROR_NETWORK_PROBLEM;
  }
  LOG(ERROR) << "kinit failed with exit code " << kinit_cmd.GetExitCode();
  return ERROR_KINIT_FAILED;
}

ErrorType GetNetError(const ProcessExecutor& executor,
                      const std::string& net_command) {
  // Handle different error cases
  const std::string& net_out = executor.GetStdout();
  const std::string& net_err = executor.GetStderr();
  const std::string error_msg("net ads " + net_command + " failed: ");

  if (Contains(net_out, kKeyJoinFailedToFindDC) ||
      Contains(net_err, kKeyNoLogonServers)) {
    LOG(ERROR) << error_msg << "network failure";
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
  if (Contains(net_out, kKeyBadMachineName)) {
    LOG(ERROR) << error_msg << "incorrect machine name";
    return ERROR_BAD_MACHINE_NAME;
  }
  if (Contains(net_out, kKeyMachineNameTooLong)) {
    LOG(ERROR) << error_msg << "machine name is too long";
    return ERROR_MACHINE_NAME_TOO_LONG;
  }
  if (Contains(net_out, kKeyUserHitJoinQuota)) {
    LOG(ERROR) << error_msg << "user joined max number of machines";
    return ERROR_USER_HIT_JOIN_QUOTA;
  }
  LOG(ERROR) << error_msg <<  "failed with exit code "
             << executor.GetExitCode();
  return ERROR_NET_FAILED;
}

ErrorType GetSmbclientError(const ProcessExecutor& smb_client_cmd) {
  const std::string& smb_client_out = smb_client_cmd.GetStdout();
  if (Contains(smb_client_out, kKeyNetworkTimeout)) {
    LOG(ERROR) << "smbclient failed - network failure";
    return ERROR_NETWORK_PROBLEM;
  }
  LOG(ERROR) << "smbclient failed with exit code "
             << smb_client_cmd.GetExitCode();
  return ERROR_SMBCLIENT_FAILED;
}

// Retrieves the name of the domain controller. If the full server name is
// 'server.realm', |domain_controller_name| is set to 'server'. Since the domain
// controller name is expected to change very rarely, this function earlies out
// and returns true if called with a non-empty |domain_controller_name|. The
// domain controller name is required for proper kerberized authentication.
bool UpdateDomainControllerName(std::string* domain_controller_name,
                                ErrorType* out_error) {
  if (!domain_controller_name->empty())
    return true;

  authpolicy::ProcessExecutor net_cmd(
      {kNetPath, "ads", "info", "-s", kSmbFilePath});
  if (!SetupJailAndRun(&net_cmd, kNetAdsSeccompFilter)) {
    *out_error = GetNetError(net_cmd, "info");
    return false;
  }
  const std::string& net_out = net_cmd.GetStdout();

  // Parse the output to find the domain controller name. Enclose in a sandbox
  // for security considerations.
  ProcessExecutor parse_cmd({ac::kParserPath, ac::kCmdParseDcName});
  parse_cmd.SetInputString(net_out);
  if (!SetupJailAndRun(&parse_cmd, kParserSeccompFilter)) {
    LOG(ERROR) << "authpolicy_parser parse_dc_name failed with exit code "
               << parse_cmd.GetExitCode();
    *out_error = ERROR_PARSE_FAILED;
    return false;
  }
  *domain_controller_name = parse_cmd.GetStdout();

  LOG(INFO) << "Found DC name = '" << *domain_controller_name << "'";
  return true;
}

// Retrieves the name of the workgroup.
bool GetWorkgroup(const ap::SambaConfig* config, std::string* workgroup,
                  ErrorType* out_error) {
  ProcessExecutor net_cmd(
      {kNetPath, "ads", "workgroup", "-s", kSmbFilePath});
  if (!SetupJailAndRun(&net_cmd, kNetAdsSeccompFilter)) {
    *out_error = GetNetError(net_cmd, "workgroup");
    return false;
  }
  const std::string& net_out = net_cmd.GetStdout();

  // Parse the output to find the workgroup. Enclose in a sandbox for security
  // considerations.
  ProcessExecutor parse_cmd({ac::kParserPath, ac::kCmdParseWorkgroup});
  parse_cmd.SetInputString(net_out);
  if (!SetupJailAndRun(&parse_cmd, kParserSeccompFilter)) {
    LOG(ERROR) << "authpolicy_parser parse_workgroup failed with exit code "
               << parse_cmd.GetExitCode();
    *out_error = ERROR_PARSE_FAILED;
    return false;
  }
  DCHECK(workgroup);
  *workgroup = parse_cmd.GetStdout();
  return true;
}

// Creates the given directory recursively and sets error message on failure.
bool CreateDirectory(const base::FilePath& dir, ErrorType* out_error) {
  base::File::Error ferror;
  if (!base::CreateDirectoryAndGetError(dir, &ferror)) {
    LOG(ERROR) << "Failed to create directory '" << dir.value()
               << "': " << base::File::ErrorToString(ferror);
    *out_error = ERROR_LOCAL_IO;
    return false;
  }
  return true;
}

// Sets file permissions for a given filepath and sets error message on failure.
bool SetFilePermissions(const base::FilePath& fp,
                        int mode,
                        ErrorType* out_error) {
  if (!base::SetPosixFilePermissions(fp, mode)) {
    LOG(ERROR) << "Failed to set permissions on '" << fp.value() << "'";
    *out_error = ERROR_LOCAL_IO;
    return false;
  }
  return true;
}

// Similar to |SetFilePermissions|, but sets permissions recursively up the path
// to |base_fp| (not including |base_fp|). Returns false if |base_fp| is not a
// parent of |fp|.
bool SetFilePermissionsRecursive(const base::FilePath& fp,
                                 const base::FilePath& base_fp,
                                 int mode,
                                 ErrorType* out_error) {
  if (!base_fp.IsParent(fp)) {
    LOG(ERROR) << "Base path '" << base_fp.value() << "' is not a parent of '"
               << fp.value() << "'";
    *out_error = ERROR_LOCAL_IO;
    return false;
  }
  for (base::FilePath curr_fp = fp; curr_fp != base_fp;
       curr_fp = curr_fp.DirName()) {
    if (!SetFilePermissions(curr_fp, mode, out_error))
      return false;
  }
  return true;
}

// Copies the machine keytab file to the state directory. The copy is owned by
// authpolicyd, so that authpolicyd_exec cannot modify it anymore.
bool SecureMachineKeyTab(ErrorType* out_error) {
  // At this point, tmp_kt_fp is rw for authpolicyd-exec only, so we, i.e.
  // user authpolicyd, cannot read it. Thus, change file permissions as
  // authpolicyd-exec user, so that the authpolicyd group can read it.
  const base::FilePath tmp_kt_fp(kMachineKtTmpFilePath);
  const base::FilePath state_kt_fp(kMachineKtStateFilePath);

  // Set group read permissions on keytab as authpolicyd-exec, so we can copy it
  // as authpolicyd (and own the copy).
  {
    ScopedAuthpolicyExecSwitch switch_scope;
    if (!SetFilePermissions(tmp_kt_fp, kFileMode_rwr, out_error))
      return false;
  }

  // Create empty file in destination directory. Note that it is created with
  // rw_r__r__ permissions.
  if (base::WriteFile(state_kt_fp, nullptr, 0) != 0) {
    LOG(ERROR) << "Failed to create file '" << state_kt_fp.value() << "'";
    *out_error = ERROR_LOCAL_IO;
    return false;
  }

  // Revoke 'read by others' permission. We could also just copy tmp_kt_fp to
  // state_kt_fp (see below) and revoke the read permission afterwards, but then
  // state_kt_fp would be readable by anyone for a split second, causing a
  // potential security risk.
  if (!SetFilePermissions(state_kt_fp, kFileMode_rwr, out_error))
    return false;

  // Now we may copy the file. The copy is owned by authpolicyd:authpolicyd.
  if (!base::CopyFile(tmp_kt_fp, state_kt_fp)) {
    PLOG(ERROR) << "Failed to copy file '" << tmp_kt_fp.value()
                << "' to '" << state_kt_fp.value() << "'";
    *out_error = ERROR_LOCAL_IO;
    return false;
  }

  // Clean up temp file (must be done as authpolicyd-exec).
  {
    ScopedAuthpolicyExecSwitch switch_scope;
    if (!base::DeleteFile(tmp_kt_fp, false)) {
      LOG(ERROR) << "Failed to delete file '" << tmp_kt_fp.value() << "'";
      *out_error = ERROR_LOCAL_IO;
      return false;
    }
  }

  return true;
}

// Writes the samba configuration file.
bool WriteSmbConf(const ap::SambaConfig* config, const std::string& workgroup,
                  ErrorType* out_error) {
  if (!config) {
    LOG(ERROR) << "Missing configuration. Must call JoinMachine first.";
    *out_error = ERROR_NOT_JOINED;
    return false;
  }

  std::string data =
      base::StringPrintf(kSmbConfData, config->machine_name().c_str(),
                         workgroup.c_str(), config->realm().c_str());
  const base::FilePath fp(kSmbFilePath);
  const int data_size = static_cast<int>(data.size());
  if (base::WriteFile(fp, data.c_str(), data_size) != data_size) {
    LOG(ERROR) << "Failed to write samba conf file '" << fp.value() << "'";
    *out_error = ERROR_LOCAL_IO;
    return false;
  }

  return true;
}

// Writes the Samba configuration file. If |workgroup| points to an empty
// string, the workgroup is queried from the server and the string is updated.
// Workgroups are only queried once a session, they are expected to change very
// rarely.
bool UpdateWorkgroupAndWriteSmbConf(const ap::SambaConfig* config,
                                    std::string* workgroup,
                                    ErrorType* out_error) {
  DCHECK(workgroup);
  if (workgroup->empty()) {
    // GetWorkgroup requires an smb.conf file, write one with empty workgroup.
    if (!WriteSmbConf(config, *workgroup, out_error) ||
        !GetWorkgroup(config, workgroup, out_error)) {
      return false;
    }
  }

  // Write smb.conf (potentially again, with valid workgroup).
  return WriteSmbConf(config, *workgroup, out_error);
}

// Writes the krb5 configuration file.
bool WriteKrb5Conf(const std::string& realm, ErrorType* out_error) {
  std::string data = base::StringPrintf(kKrb5ConfData, realm.c_str());
  const base::FilePath fp(kKrb5FilePath);
  const int data_size = static_cast<int>(data.size());
  if (base::WriteFile(fp, data.c_str(), data_size) != data_size) {
    LOG(ERROR) << "Failed to write krb5 conf file '" << fp.value() << "'";
    *out_error = ERROR_LOCAL_IO;
    return false;
  }

  return true;
}

// Writes the file with configuration information.
bool WriteConfiguration(const ap::SambaConfig* config, ErrorType* out_error) {
  DCHECK(config);
  std::string config_blob;
  if (!config->SerializeToString(&config_blob)) {
    LOG(ERROR) << "Failed to serialize configuration to string";
    *out_error = ERROR_LOCAL_IO;
    return false;
  }

  const base::FilePath fp(kConfigFilePath);
  const int config_size = static_cast<int>(config_blob.size());
  if (base::WriteFile(fp, config_blob.c_str(), config_size) != config_size) {
    LOG(ERROR) << "Failed to write configuration file '" << fp.value() << "'";
    *out_error = ERROR_LOCAL_IO;
    return false;
  }

  LOG(INFO) << "Wrote configuration file '" << fp.value() << "'";
  return true;
}

// Reads the file with configuration information.
bool ReadConfiguration(ap::SambaConfig* config) {
  const base::FilePath fp(kConfigFilePath);
  if (!base::PathExists(fp)) {
    LOG(ERROR) << "Configuration file '" << fp.value() << "' does not exist";
    return false;
  }

  std::string config_blob;
  if (!base::ReadFileToStringWithMaxSize(fp, &config_blob, kConfigSizeLimit)) {
    LOG(ERROR) << "Failed to read configuration file '" << fp.value() << "'";
    return false;
  }

  if (!config->ParseFromString(config_blob)) {
    LOG(ERROR) << "Failed to parse configuration from string";
    return false;
  }

  // Check if the config is valid.
  if (config->machine_name().empty() || config->realm().empty()) {
    LOG(ERROR) << "Configuration is invalid";
    return false;
  }

  LOG(INFO) << "Read configuration file '" << fp.value() << "'";
  return true;
}

// Calls net ads search with given |search_string| to get an objectGUID.
bool GetAccountId(const std::string& search_string, std::string* out_account_id,
                  ErrorType* out_error) {
  // Call net ads search to find the user's object GUID, which is used as
  // account id.
  ProcessExecutor net_cmd(
      {kNetPath, "ads", "search", search_string, "objectGUID", "-s",
       kSmbFilePath});
  if (!SetupJailAndRun(&net_cmd, kNetAdsSeccompFilter)) {
    *out_error = GetNetError(net_cmd, "search");
    return false;
  }
  const std::string& net_out = net_cmd.GetStdout();

  // Parse the output to find the account id. Enclose in a sandbox for security
  // considerations.
  ProcessExecutor parse_cmd({ac::kParserPath, ac::kCmdParseAccountId});
  parse_cmd.SetInputString(net_out);
  if (!SetupJailAndRun(&parse_cmd, kParserSeccompFilter)) {
    LOG(ERROR) << "Failed to get user account id. Net response: " << net_out;
    *out_error = ERROR_PARSE_FAILED;
    return false;
  }
  *out_account_id = parse_cmd.GetStdout();
  return true;
}

bool GetGpoList(const std::string& user_or_machine_name,
                ac::PolicyScope scope,
                std::string* out_gpo_list,
                ErrorType* out_error) {
  DCHECK(out_gpo_list);
  LOG(INFO) << "Getting GPO list for " << user_or_machine_name;

  // Machine names are names ending with $, anything else is a user name.
  authpolicy::ProcessExecutor net_cmd({kNetPath, "ads", "gpo", "list",
                                       user_or_machine_name, "-s",
                                       kSmbFilePath});
  if (!SetupJailAndRun(&net_cmd, kNetAdsSeccompFilter)) {
    *out_error = GetNetError(net_cmd, "gpo list");
    return false;
  }

  // GPO data is written to stderr, not stdin!
  const std::string& net_out = net_cmd.GetStderr();

  // Parse the GPO list. Enclose in a sandbox for security considerations.
  const char* cmd = scope == ac::PolicyScope::USER ? ac::kCmdParseUserGpoList
                                                   : ac::kCmdParseDeviceGpoList;
  ProcessExecutor parse_cmd({ac::kParserPath, cmd});
  parse_cmd.SetInputString(net_out);
  if (!SetupJailAndRun(&parse_cmd, kParserSeccompFilter)) {
    LOG(ERROR) << "Failed to parse GPO list";
    *out_error = ERROR_PARSE_FAILED;
    return false;
  }
  *out_gpo_list = parse_cmd.GetStdout();

  return true;
}

struct GpoPaths {
  std::string server_;    // GPO file path on server (not a local file path!).
  base::FilePath local_;  // Local GPO file path.
  GpoPaths(const std::string& server, const std::string& local)
      : server_(server), local_(local) {}
};

bool DownloadGpos(const std::string& gpo_list_blob,
                  const std::string& domain_controller_name,
                  const char* preg_dir,
                  std::vector<base::FilePath>* out_gpo_file_paths,
                  ErrorType* out_error) {
  // Parse GPO list protobuf.
  ap::GpoList gpo_list;
  if (!gpo_list.ParseFromString(gpo_list_blob)) {
    LOG(ERROR) << "Failed to read GPO list protobuf";
    return false;
  }

  if (gpo_list.entries_size() == 0) {
    LOG(INFO) << "No GPOs to download";
    return true;
  }

  // Generate all smb source and linux target directories and create targets.
  std::string smb_command = "prompt OFF;";
  std::string gpo_basepath;
  std::vector<GpoPaths> gpo_paths;
  for (int entry_idx = 0; entry_idx < gpo_list.entries_size(); ++entry_idx) {
    const ap::GpoEntry& gpo = gpo_list.entries(entry_idx);

    // Security check, make sure nobody sneaks in smbclient commands.
    if (gpo.basepath().find(';') != std::string::npos ||
        gpo.directory().find(';') != std::string::npos) {
      LOG(ERROR) << "GPO paths may not contain a ';'";
      *out_error = ERROR_BAD_GPOS;
      return false;
    }

    // All GPOs should have the same basepath, i.e. come from the same SysVol.
    if (gpo_basepath.empty()) {
      gpo_basepath = gpo.basepath();
    } else if (!base::EqualsCaseInsensitiveASCII(gpo_basepath,
                                                 gpo.basepath())) {
      LOG(ERROR) << "Inconsistent base path '" << gpo_basepath << "' != '"
                 << gpo.basepath() << "'";
      *out_error = ERROR_BAD_GPOS;
      return false;
    }

    // Figure out local (Linux) and remote (smb) directories.
    std::string smb_dir =
      base::StringPrintf("\\%s\\%s", gpo.directory().c_str(), preg_dir);
    std::string linux_dir = kGpoLocalDir + smb_dir;
    std::replace(linux_dir.begin(), linux_dir.end(), '\\', '/');

    // Make local directory.
    const base::FilePath linux_dir_fp(linux_dir);
    if (!CreateDirectory(linux_dir_fp, out_error))
      return false;

    // Set group rwx permissions recursively, so that smbclient can write GPOs
    // there and the parser tool can read the GPOs later.
    if (!SetFilePermissionsRecursive(linux_dir_fp, base::FilePath(kSambaTmpDir),
                                     kFileMode_rwxrwx, out_error)) {
      return false;
    }

    // Build command for smbclient.
    smb_command += base::StringPrintf("cd %s;lcd %s;mget %s;", smb_dir.c_str(),
                                      linux_dir.c_str(), kPRegFileName);

    // Record output file paths.
    gpo_paths.push_back(GpoPaths(smb_dir + "\\" + kPRegFileName,
                                 linux_dir + "/" + kPRegFileName));

    // Delete any preexisting policy file. Otherwise, if downloading the file
    // failed, we wouldn't realize it and use a stale version.
    if (base::PathExists(gpo_paths.back().local_) &&
        !base::DeleteFile(gpo_paths.back().local_, false)) {
      LOG(ERROR) << "Failed to delete old GPO file '"
                 << gpo_paths.back().local_.value().c_str() << "'";
      return false;
    }
  }

  std::string service = base::StringPrintf(
      "//%s.%s", domain_controller_name.c_str(), gpo_basepath.c_str());

  // Download GPO into local directory.
  ProcessExecutor smb_client_cmd(
      {kSmbClientPath, service, "-s", kSmbFilePath, "-c", smb_command, "-k"});
  if (!SetupJailAndRun(&smb_client_cmd, kSmbClientSeccompFilter)) {
    // The exit code of smbclient corresponds to the LAST command issued. Thus,
    // Execute() might fail if the last GPO file is missing. However, we handle
    // this below (not an error), so only error out here on internal errors.
    if (smb_client_cmd.GetExitCode() ==
        ProcessExecutor::kExitCodeInternalError) {
      *out_error = GetSmbclientError(smb_client_cmd);
      return false;
    }
  }
  // Note that the errors are in stdout and the output is in stderr :-/
  const std::string& smbclient_out_lower =
      base::ToLowerASCII(smb_client_cmd.GetStdout());

  // Make sure the GPO files actually downloaded.
  DCHECK(out_gpo_file_paths);
  for (const GpoPaths& gpo_path : gpo_paths) {
    if (base::PathExists(gpo_path.local_)) {
      out_gpo_file_paths->push_back(gpo_path.local_);
    } else {
      // Gracefully handle non-existing GPOs. Testing revealed these cases do
      // exist, see crbug.com/680921.
      const std::string no_file_error_key(
        base::ToLowerASCII(kKeyNoSuchFile + gpo_path.server_));
      if (Contains(smbclient_out_lower, no_file_error_key)) {
        LOG(WARNING) << "Ignoring missing preg file '"
                     << gpo_path.local_.value() << "'";
      } else {
        LOG(ERROR) << "Failed to download preg file '"
                   << gpo_path.local_.value() << "'";
        *out_error = ERROR_SMBCLIENT_FAILED;
        return false;
      }
    }
  }

  return true;
}

// Parse GPOs and store them in user/device policy protobufs.
bool ParseGposIntoProtobuf(const std::vector<base::FilePath>& gpo_file_paths,
                           const char* parser_cmd_string,
                           std::string* out_policy_blob,
                           ErrorType* out_error) {
  // Convert file paths to proto blob.
  std::string gpo_file_paths_blob;
  ap::FilePathList fp_proto;
  for (const auto& fp : gpo_file_paths)
    *fp_proto.add_entries() = fp.value();
  if (!fp_proto.SerializeToString(&gpo_file_paths_blob)) {
    LOG(ERROR) << "Failed to serialize policy file paths to protobuf";
    *out_error = ERROR_PARSE_PREG_FAILED;
    return false;
  }

  // Load GPOs into protobuf. Enclose in a sandbox for security considerations.
  ProcessExecutor parse_cmd({ac::kParserPath, parser_cmd_string});
  parse_cmd.SetInputString(gpo_file_paths_blob);
  if (!SetupJailAndRun(&parse_cmd, kParserSeccompFilter)) {
    LOG(ERROR) << "Failed to parse preg files";
    *out_error = ERROR_PARSE_PREG_FAILED;
    return false;
  }
  *out_policy_blob = parse_cmd.GetStdout();
  return true;
}

}  // namespace

bool SambaInterface::Initialize(bool expect_config) {
  // Need to create samba dirs since samba can't create dirs recursively...
  ErrorType error = ERROR_NONE;
  for (const auto& dir_and_mode : kSambaDirsAndMode) {
    const base::FilePath dir(dir_and_mode.first);
    const int mode = dir_and_mode.second;
    if (!CreateDirectory(dir, &error) ||
        !SetFilePermissions(dir, mode, &error)) {
      LOG(ERROR) << "Failed to initialize SambaInterface";
      return false;
    }
  }

  if (expect_config) {
    config_ = base::MakeUnique<ap::SambaConfig>();
    if (!ReadConfiguration(config_.get())) {
      LOG(ERROR) << "Failed to initialize SambaInterface";
      config_.reset();
      return false;
    }
  }

  // Load debug flags file if present. Always CHECK() the flags, even in
  // release, to catch uninitialized variables.
  CHECK(!s_disable_seccomp_filters);
  CHECK(!s_log_seccomp_filters);
  std::string flags;
  if (base::ReadFileToString(base::FilePath(kFlagsFilePath), &flags)) {
    if (Contains(flags, kFlagDisableSeccomp)) {
      LOG(WARNING) << "Seccomp filters disabled";
      s_disable_seccomp_filters = true;
    }
    if (Contains(flags, kFlagLogSeccomp)) {
      LOG(WARNING) << "Logging seccomp filter failures";
      s_log_seccomp_filters = true;
    }
  }

  return true;
}

bool SambaInterface::AuthenticateUser(const std::string& user_principal_name,
                                      int password_fd,
                                      std::string* out_account_id,
                                      ErrorType* out_error) {
  // Split user_principal_name into parts and normalize.
  std::string user_name, realm, workgroup, normalized_upn;
  if (!ai::ParseUserPrincipalName(user_principal_name, &user_name, &realm,
                                  &normalized_upn, out_error))
    return false;

  // Write krb5 configuration file.
  if (!WriteKrb5Conf(realm, out_error))
    return false;

  // Write samba configuration file.
  if (!UpdateWorkgroupAndWriteSmbConf(config_.get(), &workgroup_, out_error))
    return false;

  // Call kinit to get the Kerberos ticket-granting-ticket.
  ProcessExecutor kinit_cmd({kKInitPath, normalized_upn});
  kinit_cmd.SetInputFile(password_fd);
  kinit_cmd.SetEnv(kKrb5ConfEnvKey, kKrb5ConfEnvValue);  // Kerberos config.
  if (!SetupJailAndRun(&kinit_cmd, kKInitSeccompFilter)) {
    *out_error = GetKinitError(kinit_cmd);
    return false;
  }

  // Get a unique id for the user account. Search by sAMAccountName first since
  // that's what kinit/Windows prefer and if that fails, search by UPN. The name
  // part of the principal name can be different from the sAMAccountName!
  if (!GetAccountId(
      base::StringPrintf("(sAMAccountName=%s)", user_name.c_str()),
      out_account_id, out_error) && *out_error == ERROR_PARSE_FAILED) {
    LOG(WARNING) << "Object GUID not found by sAMAccountName. "
                 << "Trying userPrincipalName.";
    *out_error = ERROR_NONE;
    if (!GetAccountId(
        base::StringPrintf("(userPrincipalName=%s)", normalized_upn.c_str()),
        out_account_id, out_error)) {
      return false;
    }
  }

  // Store user name for further reference.
  const std::string account_id_key(kActiveDirectoryPrefix + *out_account_id);
  account_id_key_user_name_map_[account_id_key] = user_name;
  return true;
}

bool SambaInterface::JoinMachine(const std::string& machine_name,
                                 const std::string& user_principal_name,
                                 int password_fd,
                                 ErrorType* out_error) {
  // Split user principal name into parts.
  std::string user_name, realm, normalized_upn;
  if (!ai::ParseUserPrincipalName(user_principal_name, &user_name, &realm,
                                  &normalized_upn, out_error))
    return false;

  // Create config.
  std::unique_ptr<ap::SambaConfig> config = base::MakeUnique<ap::SambaConfig>();
  config->set_machine_name(base::ToUpperASCII(machine_name));
  config->set_realm(realm);

  // Write samba configuration. Will query the workgroup.
  workgroup_.clear();
  if (!UpdateWorkgroupAndWriteSmbConf(config.get(), &workgroup_, out_error))
    return false;

  // Call net ads join to join the machine to the Active Directory domain.
  ProcessExecutor net_cmd(
      {kNetPath, "ads", "join", "-U", normalized_upn, "-s", kSmbFilePath});
  net_cmd.SetInputFile(password_fd);
  net_cmd.SetEnv(kMachineKTEnvKey, kMachineKTEnvValueTmp);  // Keytab file path.
  if (!SetupJailAndRun(&net_cmd, kNetAdsSeccompFilter)) {
    *out_error = GetNetError(net_cmd, "join");
    return false;
  }

  // Prevent that authpolicyd-exec can make changes to the keytab file.
  if (!SecureMachineKeyTab(out_error))
    return false;

  // Store configuration for subsequent runs of the daemon.
  if (!WriteConfiguration(config.get(), out_error))
    return false;

  // Only if everything worked out, keep the config.
  config_ = std::move(config);
  retry_machine_kinit_ = true;
  return true;
}

bool SambaInterface::FetchUserGpos(const std::string& account_id_key,
                                   std::string* out_policy_blob,
                                   ErrorType* out_error) {
  // Get user name from account id key (must be logged in to fetch user policy).
  std::unordered_map<std::string, std::string>::const_iterator iter =
      account_id_key_user_name_map_.find(account_id_key);
  if (iter == account_id_key_user_name_map_.end()) {
    LOG(ERROR) << "User not logged in. Please call AuthenticateUser first.";
    *out_error = ERROR_NOT_LOGGED_IN;
    return false;
  }
  const std::string& user_name = iter->second;

  // Write samba configuration file.
  if (!UpdateWorkgroupAndWriteSmbConf(config_.get(), &workgroup_, out_error))
    return false;

  // Make sure we have the domain controller name.
  if (!UpdateDomainControllerName(&domain_controller_name_, out_error))
    return false;

  // FetchDeviceGpos writes a krb5.conf here. For user policy, there's no need
  // to do that here since we're reusing the TGT generated in AuthenticateUser.

  // Get the list of GPOs for the given user name.
  std::string gpo_list_blob;
  if (!GetGpoList(user_name, ac::PolicyScope::USER, &gpo_list_blob, out_error))
    return false;

  // Download GPOs from Active Directory server.
  std::vector<base::FilePath> gpo_file_paths;
  if (!DownloadGpos(gpo_list_blob, domain_controller_name_, kPRegUserDir,
                    &gpo_file_paths, out_error))
    return false;

  // Parse GPOs and store them in a user policy protobuf.
  if (!ParseGposIntoProtobuf(gpo_file_paths, ac::kCmdParseUserPreg,
                             out_policy_blob, out_error))
    return false;

  return true;
}

bool SambaInterface::FetchDeviceGpos(std::string* out_policy_blob,
                                     ErrorType* out_error) {
  // Write samba configuration file.
  if (!UpdateWorkgroupAndWriteSmbConf(config_.get(), &workgroup_, out_error))
    return false;

  // Make sure we have the domain controller name.
  if (!UpdateDomainControllerName(&domain_controller_name_, out_error))
    return false;

  // Write krb5 configuration file.
  DCHECK(config_.get());
  if (!WriteKrb5Conf(config_->realm(), out_error))
    return false;

  // Call kinit to get the Kerberos ticket-granting-ticket.
  ProcessExecutor kinit_cmd(
      {kKInitPath, config_->machine_name() + "$@" + config_->realm(), "-k"});
  kinit_cmd.SetEnv(kKrb5ConfEnvKey, kKrb5ConfEnvValue);  // Kerberos config.
  kinit_cmd.SetEnv(kMachineKTEnvKey, kMachineKTEnvValueState);  // Keytab file.

  // The first device policy fetch after joining Active Directory can be very
  // slow because machine credentials need to propagate through the AD
  // deployment.
  bool success = false;
  const int max_tries = (retry_machine_kinit_ ? kMachineKinitMaxRetries : 1);
  for (int tries = 0; tries < max_tries; ++tries) {
    success = SetupJailAndRun(&kinit_cmd, kKInitSeccompFilter);
    if (success)
      break;
    *out_error = GetKinitError(kinit_cmd);
    if (*out_error != ERROR_BAD_USER_NAME && *out_error != ERROR_BAD_PASSWORD)
      break;
    base::PlatformThread::Sleep(
        base::TimeDelta::FromSeconds(kMachineKinitRetryWaitSeconds));
  }
  retry_machine_kinit_ = false;
  if (!success)
    return false;

  // Get the list of GPOs for the machine.
  std::string gpo_list_blob;
  if (!GetGpoList(config_->machine_name() + "$", ac::PolicyScope::MACHINE,
                  &gpo_list_blob, out_error))
    return false;

  // Download GPOs from Active Directory server.
  std::vector<base::FilePath> gpo_file_paths;
  if (!DownloadGpos(gpo_list_blob, domain_controller_name_, kPRegDeviceDir,
                    &gpo_file_paths, out_error))
    return false;

  // Parse GPOs and store them in a device policy protobuf.
  if (!ParseGposIntoProtobuf(gpo_file_paths, ac::kCmdParseDevicePreg,
                             out_policy_blob, out_error))
    return false;

  return true;
}

}  // namespace authpolicy
