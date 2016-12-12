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

#include "authpolicy/errors.h"
#include "authpolicy/process_executor.h"
#include "authpolicy/samba_interface_internal.h"

namespace ai = authpolicy::internal;
namespace ap = authpolicy::protos;

namespace authpolicy {
namespace {

// Temporary data. Note: Use a #define, so we can concat strings below.
#define TMP_DIR "/tmp/authpolicyd"

// Persisted samba data.
#define STATE_DIR "/var/lib/authpolicyd"

// Temporary samba data.
#define SAMBA_TMP_DIR TMP_DIR "/samba"

// Kerberos configuration file.
#define KRB5_FILE_PATH TMP_DIR "/krb5.conf"

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
    "\t# Only allow AES. (No DES, no RC4.)\n"
    "\tdefault_tgs_enctypes = aes256-cts-hmac-sha1-96 aes128-cts-hmac-sha1-96\n"
    "\tdefault_tkt_enctypes = aes256-cts-hmac-sha1-96 aes128-cts-hmac-sha1-96\n"
    "\tpermitted_enctypes = aes256-cts-hmac-sha1-96 aes128-cts-hmac-sha1-96\n"
    // Prune weak ciphers from the above list. With current settings it’s a
    // no-op, but still.
    "\tallow_weak_crypto = false\n"
    // Default is 300 seconds, but we might add a policy for that in the future.
    "\tclockskew = 300\n";

// Directories to create (Samba fails to do it on its own).
const char* kSambaDirs[] = {
  SAMBA_TMP_DIR "/lock",
  SAMBA_TMP_DIR "/cache",
  SAMBA_TMP_DIR "/state",
  SAMBA_TMP_DIR "/private"
};

// Location to download GPOs to.
const char kGpoLocalDir[] = SAMBA_TMP_DIR "/cache/gpo_cache/";

// Directory / filenames for user and device policy.
const char kPRegUserDir[] = "User";
const char kPRegDeviceDir[] = "Machine";
const char kPRegFileName[] = "Registry.pol";

// Configuration files.
const char kSmbFilePath[] = SAMBA_TMP_DIR "/smb.conf";
const char kKrb5FilePath[] = KRB5_FILE_PATH;
const char kConfigPath[] = STATE_DIR "/config.dat";

// Size limit when loading the config file (256 kb).
const size_t kConfigSizeLimit = 256 * 1024;

// Env variable for krb5.conf file.
const char kKrb5ConfEnvKey[] = "KRB5_CONFIG";
const char kKrb5ConfEnvValue[] = "FILE:" KRB5_FILE_PATH;

// Env variable for machine keytab (machine password for getting device policy).
const char kMachineEnvKey[] = "KRB5_KTNAME";
const char kMachineEnvValue[] = "FILE:" STATE_DIR "/krb5_machine.keytab";

// Executable paths. For security reasons use absolute file paths!
const char kNetPath[] = "/usr/bin/net";
const char kKInitPath[] = "/usr/bin/kinit";
const char kSmbClientPath[] = "/usr/bin/smbclient";

// 'net ads gpo list'' tokens
const char kGpoToken_Separator[] = "---------------------";
const char kGpoToken_Name[] = "name";
const char kGpoToken_Filesyspath[] = "filesyspath";
const char kGpoToken_VersionUser[] = "version_user";
const char kGpoToken_VersionMachine[] = "version_machine";

#undef KRB5_FILE_PATH
#undef SAMBA_TMP_DIR
#undef STATE_DIR
#undef TMP_DIR

// Retrieves the name of the domain controller. If the full server name is
// 'myserver.workgroup.domain', |domain_controller_name| is set to 'myserver'.
// Since the domain controller name is not expected to change, this function
// earlies out and returns true if called with a non-empty
// |domain_controller_name|. The domain controller name is required for proper
// kerberized authentication.
bool UpdateDomainControllerName(std::string* domain_controller_name,
                                const char** out_error_code) {
  if (!domain_controller_name->empty())
    return true;
  authpolicy::ProcessExecutor net_cmd(
      {kNetPath, "ads", "info", "-s", kSmbFilePath});
  if (!net_cmd.Execute()) {
    LOG(ERROR) << "net ads info failed with exit code "
               << net_cmd.GetExitCode();
    *out_error_code = errors::kNetAdsInfoFailed;
    return false;
  }

  const std::string& net_out = net_cmd.GetStdout();
  if (!ai::FindToken(
      net_out, ':', "LDAP server name", domain_controller_name)) {
    LOG(ERROR) << "Failed to find 'LDAP server name' in net response '"
               << net_out << "'.";
    *out_error_code = errors::kParseNetAdsInfoFailed;
    return false;
  }

  size_t dot_pos = domain_controller_name->find('.');
  *domain_controller_name = domain_controller_name->substr(0, dot_pos);

  LOG(INFO) << "Found DC name = '" << *domain_controller_name << "'";
  return true;
}

// Creates the given directory recursively and sets error message on failure.
bool CreateDirectory(const char* dir, const char** out_error_code) {
  base::FilePath fp(dir);
  base::File::Error ferror;
  if (!base::CreateDirectoryAndGetError(fp, &ferror)) {
    LOG(ERROR) << "Failed to create directory '" << dir
               << "': " << base::File::ErrorToString(ferror);
    *out_error_code = errors::kCreateDirFailed;
    return false;
  }
  return true;
}

// Writes the samba configuration file.
bool WriteSmbConf(const ap::SambaConfig* config, const char** out_error_code) {
  if (!config) {
    LOG(ERROR) << "No configuration to write. Must call JoinMachine first.";
    *out_error_code = errors::kWriteSmbConfFailed;
    return false;
  }

  std::string data =
      base::StringPrintf(kSmbConfData, config->machine_name().c_str(),
                         config->workgroup().c_str(), config->realm().c_str());
  base::FilePath fp(kSmbFilePath);
  if (base::WriteFile(fp, data.c_str(), data.size()) <= 0) {
    LOG(ERROR) << "Failed to write samba conf file '" << fp.value() << "'.";
    *out_error_code = errors::kWriteSmbConfFailed;
    return false;
  }

  return true;
}

// Writes the krb5 configuration file.
bool WriteKrb5Conf(const char** out_error_code) {
  base::FilePath fp(kKrb5FilePath);
  if (base::WriteFile(fp, kKrb5ConfData, strlen(kKrb5ConfData)) <= 0) {
    LOG(ERROR) << "Failed to write krb5 conf file '" << fp.value() << "'.";
    *out_error_code = errors::kWriteKrb5ConfFailed;
    return false;
  }

  return true;
}

// Writes the file with configuration information.
bool WriteConfiguration(const ap::SambaConfig* config,
                        const char** out_error_code) {
  DCHECK(config);
  std::string config_blob;
  if (!config->SerializeToString(&config_blob)) {
    LOG(ERROR) << "Failed to serialize configuration to string";
    *out_error_code = errors::kWriteConfigFailed;
    return false;
  }

  base::FilePath fp(kConfigPath);
  if (base::WriteFile(fp, config_blob.c_str(), config_blob.size()) <= 0) {
    LOG(ERROR) << "Failed to write configuration file '" << fp.value() << "'.";
    *out_error_code = errors::kWriteConfigFailed;
    return false;
  }

  LOG(INFO) << "Wrote configuration file '" << fp.value() << "'.";
  return true;
}

// Reads the file with configuration information.
bool ReadConfiguration(ap::SambaConfig* config) {
  base::FilePath fp(kConfigPath);
  if (!base::PathExists(fp)) {
    LOG(ERROR) << "Configuration file '" << fp.value() << "' does not exist.";
    return false;
  }

  std::string config_blob;
  if (!base::ReadFileToStringWithMaxSize(fp, &config_blob, kConfigSizeLimit)) {
    LOG(ERROR) << "Failed to read configuration file '" << fp.value() << "'.";
    return false;
  }

  if (!config->ParseFromString(config_blob)) {
    LOG(ERROR) << "Failed to parse configuration from string";
    return false;
  }

  // Check if the config is valid.
  if (config->machine_name().empty() || config->workgroup().empty() ||
      config->realm().empty()) {
    LOG(ERROR) << "Configuration is invalid";
    return false;
  }

  LOG(INFO) << "Read configuration file '" << fp.value() << "'.";
  return true;
}

struct GpoEntry {
  GpoEntry() { Clear(); }

  void Clear() {
    name.clear();
    filesyspath.clear();
    version_user = 0;
    version_machine = 0;
  }

  bool IsValid() const {
    return !name.empty() && !filesyspath.empty() &&
           !(version_user == 0 && version_machine == 0);
  }

  bool IsEmpty() const {
    return name.empty() && filesyspath.empty() && version_user == 0 &&
           version_machine == 0;
  }

  void Log() const {
    LOG(INFO) << "  Name:            " << name;
    LOG(INFO) << "  Filesyspath:     " << filesyspath;
    LOG(INFO) << "  Version-User:    " << version_user;
    LOG(INFO) << "  Version-Machine: " << version_machine;
  }

  std::string name;
  std::string filesyspath;
  int version_user;
  int version_machine;
};

void PushGpo(const GpoEntry& gpo, std::vector<GpoEntry>* gpo_list) {
  if (gpo.IsValid()) {
    gpo_list->push_back(gpo);
  } else if (!gpo.IsEmpty()) {
    LOG(INFO) << "Ignoring invalid GPO";
    gpo.Log();
  }
}

bool GetGpoList(const std::string& user_or_machine_name,
                std::vector<GpoEntry>* out_gpo_list,
                const char** out_error_code) {
  DCHECK(out_gpo_list);
  LOG(INFO) << "Getting GPO list for " << user_or_machine_name;

  // Machine names are names ending with $, anything else is a user name.
  authpolicy::ProcessExecutor net_cmd({kNetPath, "ads", "gpo", "list",
                                       user_or_machine_name, "-s",
                                       kSmbFilePath});
  if (!net_cmd.Execute()) {
    LOG(ERROR) << "net ads gpo list failed with exit code "
               << net_cmd.GetExitCode();
    *out_error_code = errors::kNetAdsGpoListFailed;
    return false;
  }

  // GPO data is written to stderr, not stdin!
  const std::string& net_out = net_cmd.GetStderr();

  GpoEntry gpo;
  std::vector<std::string> lines = base::SplitString(
      net_out, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  LOG(INFO) << "Parsing GPO list (" << lines.size() << " lines)";
  bool found_separator = false;
  for (const std::string& line : lines) {
    if (line.find(kGpoToken_Separator) != std::string::npos) {
      // Separator between entries. Process last gpo if any.
      PushGpo(gpo, out_gpo_list);
      gpo.Clear();
      found_separator = true;
    } else {
      // Collect data
      std::vector<std::string> tokens = base::SplitString(
          line, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

      if (tokens.size() == 2) {
        bool already_set = false;
        if (tokens[0] == kGpoToken_Name) {
          already_set = !gpo.name.empty();
          gpo.name = tokens[1];
        } else if (tokens[0] == kGpoToken_Filesyspath) {
          already_set = !gpo.filesyspath.empty();
          gpo.filesyspath = tokens[1];
        } else if (tokens[0] == kGpoToken_VersionUser) {
          already_set = gpo.version_user != 0;
          // Expected to return false since token looks like '1 (0x00000001)'.
          base::StringToInt(tokens[1], &gpo.version_user);
        } else if (tokens[0] == kGpoToken_VersionMachine) {
          already_set = gpo.version_machine != 0;
          // Expected to return false since token looks like '1 (0x00000001)'.
          base::StringToInt(tokens[1], &gpo.version_machine);
        }

        // Sanity check that we don't miss separators between GPOs.
        if (already_set) {
          LOG(ERROR) << "Error parsing net ads gpo list result. Net response: "
                     << net_out;
          *out_error_code = errors::kParseGpoFailed;
          return false;
        }
      }
    }
  }

  // Just in case there's no separator in the end.
  PushGpo(gpo, out_gpo_list);

  if (!found_separator) {
    // This usually happens when something went wrong, e.g. connection error.
    LOG(ERROR) << "Failed to get GPO list. Net response: " << net_out;
    *out_error_code = errors::kParseGpoFailed;
    return false;
  }

  LOG(INFO) << "Found " << out_gpo_list->size() << " GPOs.";
  for (size_t n = 0; n < out_gpo_list->size(); ++n) {
    LOG(INFO) << n + 1 << ")";
    (*out_gpo_list)[n].Log();
  }

  return true;
}

bool DownloadGpo(const GpoEntry& gpo,
                 const std::string& domain_controller_name,
                 const char* preg_dir,
                 std::vector<base::FilePath>* out_gpo_file_paths,
                 const char** out_error_code) {
  LOG(INFO) << "Downloading GPO " << gpo.name;

  // Split the filesyspath, e.g.
  //   \\chrome.lan\SysVol\chrome.lan\Policies\{3507856D-B824-4417-8CD8-CF144DC5CC3A}
  // into \\chrome.lan\SysVol and the directory (rest).
  std::vector<std::string> file_parts = base::SplitString(
      gpo.filesyspath, "\\/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (file_parts.size() < 4 || !file_parts[0].empty() ||
      !file_parts[1].empty()) {
    LOG(ERROR) << "Failed to split filesyspath '" << gpo.filesyspath
                << "' into service and directory parts.";
    *out_error_code = errors::kParseGpoPathFailed;
    return false;
  }
  std::string service =
      base::StringPrintf("//%s.%s/%s", domain_controller_name.c_str(),
                          file_parts[2].c_str(), file_parts[3].c_str());
  file_parts =
      std::vector<std::string>(file_parts.begin() + 4, file_parts.end());
  std::string smb_dir = base::JoinString(file_parts, "\\") + "\\" + preg_dir;
  std::string linux_dir = kGpoLocalDir + smb_dir;
  std::replace(linux_dir.begin(), linux_dir.end(), '\\', '/');

  // Make local directory.
  if (!CreateDirectory(linux_dir.c_str(), out_error_code)) {
    LOG(ERROR) << "Failed to create GPO directory '" << linux_dir << "'.";
    return false;
  }

  // Download GPO into local directory.
  authpolicy::ProcessExecutor smb_client_cmd(
      {kSmbClientPath, service, "-D", smb_dir, "-s", kSmbFilePath, "-c",
       base::StringPrintf("prompt OFF;lcd %s;mget %s", linux_dir.c_str(),
                          kPRegFileName),
       "-k"});
  if (!smb_client_cmd.Execute()) {
    LOG(ERROR) << "smbclient failed with exit code "
                << smb_client_cmd.GetExitCode();
    *out_error_code = errors::kSmbClientFailed;
    return false;
  }

  // Make sure the file actually downloaded and add it to the output list.
  base::FilePath fp(linux_dir + "/" + kPRegFileName);
  if (!base::PathExists(fp)) {
    LOG(ERROR) << "Failed to download preg file '" << fp.value() << "'.";
    *out_error_code = errors::kDownloadGpoFailed;
    return false;
  }

  DCHECK(out_gpo_file_paths);
  out_gpo_file_paths->push_back(fp);
  return true;
}

}  // namespace

bool SambaInterface::Initialize(bool expect_config) {
  // Need to create samba dirs since samba can't create dirs recursively...
  const char* error_code;
  for (size_t n = 0; n < arraysize(kSambaDirs); ++n) {
    if (!CreateDirectory(kSambaDirs[n], &error_code)) {
      LOG(ERROR) << "Failed to initialize SambaInterface. Cannot create "
                 << "directory '" << kSambaDirs[n] << "'.";
      return false;
    }
  }

  if (expect_config) {
    config_ = base::MakeUnique<ap::SambaConfig>();
    if (!ReadConfiguration(config_.get())) {
      LOG(ERROR) << "Failed to initialize SambaInterface. Failed to load "
                 << "configuration file.";
      config_.reset();
      return false;
    }
  }

  return true;
}

bool SambaInterface::AuthenticateUser(const std::string& user_principal_name,
                                      int password_fd,
                                      std::string* out_account_id,
                                      const char** out_error_code) {
  // Split user_principal_name into parts and normalize.
  std::string user_name, realm, workgroup, normalized_upn;
  if (!ai::ParseUserPrincipalName(user_principal_name, &user_name, &realm,
                                  &workgroup, &normalized_upn, out_error_code))
    return false;

  // Write krb5 configuration file.
  if (!WriteKrb5Conf(out_error_code))
    return false;

  // Write samba configuration file.
  if (!WriteSmbConf(config_.get(), out_error_code))
    return false;

  // Call kinit to get the Kerberos ticket-granting-ticket.
  ProcessExecutor kinit_cmd({kKInitPath, normalized_upn});
  kinit_cmd.SetInputFile(password_fd);
  kinit_cmd.SetEnv(kKrb5ConfEnvKey, kKrb5ConfEnvValue);  // Kerberos config.
  if (!kinit_cmd.Execute()) {
    LOG(ERROR) << "kinit failed with exit code " << kinit_cmd.GetExitCode();
    *out_error_code = errors::kKInitFailed;
    return false;
  }

  // Call net ads search to find the user's object GUID, which is used as
  // account id.
  ProcessExecutor net_cmd(
      {kNetPath, "ads", "search",
       base::StringPrintf("(userPrincipalName=%s)", normalized_upn.c_str()),
       "objectGUID", "-s", kSmbFilePath});
  if (!net_cmd.Execute()) {
    LOG(ERROR) << "net ads search failed with exit code "
               << net_cmd.GetExitCode();
    *out_error_code = errors::kNetAdsSearchFailed;
    return false;
  }

  // Parse net output to get user's account id.
  const std::string& net_out = net_cmd.GetStdout();
  if (!ai::FindToken(net_out, ':', "objectGUID", out_account_id)) {
    LOG(ERROR) << "Failed to get user account id. Net response: " << net_out;
    *out_error_code = errors::kParseNetAdsSearchFailed;
    return false;
  }

  // Store user name for further reference.
  account_id_user_name_map_[*out_account_id] = user_name;
  return true;
}

bool SambaInterface::JoinMachine(const std::string& machine_name,
                                 const std::string& user_principal_name,
                                 int password_fd, const char** out_error_code) {
  // Split user principal name into parts.
  std::string user_name, realm, workgroup, normalized_upn;
  if (!ai::ParseUserPrincipalName(user_principal_name, &user_name, &realm,
                                  &workgroup, &normalized_upn, out_error_code))
    return false;

  // Create config.
  std::unique_ptr<ap::SambaConfig> config = base::MakeUnique<ap::SambaConfig>();
  config->set_machine_name(base::ToUpperASCII(machine_name));
  config->set_workgroup(workgroup);
  config->set_realm(realm);

  // Write samba configuration file.
  if (!WriteSmbConf(config.get(), out_error_code))
    return false;

  // Call net ads join to join the Active Directory domain.
  ProcessExecutor net_cmd(
      {kNetPath, "ads", "join", "-U", normalized_upn, "-s", kSmbFilePath});
  net_cmd.SetInputFile(password_fd);
  net_cmd.SetEnv(kMachineEnvKey, kMachineEnvValue);  // Keytab file path.
  if (!net_cmd.Execute()) {
    LOG(ERROR) << "net ads join failed with exit code "
               << net_cmd.GetExitCode();
    *out_error_code = errors::kNetAdsJoinFailed;
    return false;
  }

  // Store configuration for subsequent runs of the daemon.
  if (!WriteConfiguration(config.get(), out_error_code))
    return false;

  // Only if everything worked out, keep the config.
  config_ = std::move(config);
  return true;
}

bool SambaInterface::FetchUserGpos(const std::string& account_id,
    std::vector<base::FilePath>* out_gpo_file_paths,
    const char** out_error_code) {
  // Get user name from account id (must be logged in to fetch user policy).
  std::unordered_map<std::string, std::string>::const_iterator iter =
      account_id_user_name_map_.find(account_id);
  if (iter == account_id_user_name_map_.end()) {
    LOG(ERROR) << "No user logged in. Please call AuthenticateUser first.";
    *out_error_code = errors::kNotLoggedIn;
    return false;
  }
  const std::string& user_name = iter->second;

  // Write samba configuration file.
  if (!WriteSmbConf(config_.get(), out_error_code))
    return false;

  // Make sure we have the domain controller name.
  if (!UpdateDomainControllerName(&domain_controller_name_, out_error_code))
    return false;

  // Get the list of GPOs for the given user name.
  std::vector<GpoEntry> gpo_list;
  if (!GetGpoList(user_name, &gpo_list, out_error_code))
    return false;

  // Download GPOs.
  for (const GpoEntry& gpo : gpo_list) {
    // If version_user == 0, there's no user policy stored in that GPO.
    if (gpo.version_user == 0) {
      LOG(INFO) << "Filtered out GPO " << gpo.name << " (version_user is 0)";
      continue;
    }

    if (!DownloadGpo(gpo, domain_controller_name_, kPRegUserDir,
                     out_gpo_file_paths, out_error_code))
      return false;
  }

  return true;
}

bool SambaInterface::FetchDeviceGpos(
    std::vector<base::FilePath>* out_gpo_file_paths,
    const char** out_error_code) {
  // Write samba configuration file.
  if (!WriteSmbConf(config_.get(), out_error_code))
    return false;

  // Make sure we have the domain controller name.
  if (!UpdateDomainControllerName(&domain_controller_name_, out_error_code))
    return false;

  // Write krb5 configuration file.
  if (!WriteKrb5Conf(out_error_code))
    return false;

  // Call kinit to get the Kerberos ticket-granting-ticket.
  ProcessExecutor kinit_cmd(
      {kKInitPath, config_->machine_name() + "$@" + config_->realm(), "-k"});
  kinit_cmd.SetEnv(kKrb5ConfEnvKey, kKrb5ConfEnvValue);  // Kerberos config.
  kinit_cmd.SetEnv(kMachineEnvKey, kMachineEnvValue);    // Keytab file path.
  if (!kinit_cmd.Execute()) {
    LOG(ERROR) << "kinit failed with exit code " << kinit_cmd.GetExitCode();
    *out_error_code = errors::kKInitFailed;
    return false;
  }

  // Get the list of GPOs for the machine.
  std::vector<GpoEntry> gpo_list;
  if (!GetGpoList(config_->machine_name() + "$", &gpo_list, out_error_code))
    return false;

  // Download GPOs.
  for (const GpoEntry& gpo : gpo_list) {
    // If version_machine == 0, there's no device policy stored in that GPO.
    if (gpo.version_machine == 0) {
      LOG(INFO) << "Filtered out GPO " << gpo.name << " (version_machine is 0)";
      continue;
    }

    if (!DownloadGpo(gpo, domain_controller_name_, kPRegDeviceDir,
                     out_gpo_file_paths, out_error_code))
      return false;
  }

  return true;
}

}  // namespace authpolicy
