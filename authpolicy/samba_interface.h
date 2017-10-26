// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AUTHPOLICY_SAMBA_INTERFACE_H_
#define AUTHPOLICY_SAMBA_INTERFACE_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <base/files/file_path.h>
#include <base/macros.h>
#include <base/memory/ref_counted.h>
#include <dbus/authpolicy/dbus-constants.h>

#include "authpolicy/authpolicy_flags.h"
#include "authpolicy/authpolicy_metrics.h"
#include "authpolicy/constants.h"
#include "authpolicy/jail_helper.h"
#include "authpolicy/path_service.h"
#include "authpolicy/proto_bindings/active_directory_info.pb.h"
#include "authpolicy/samba_helper.h"
#include "authpolicy/tgt_manager.h"
#include "bindings/authpolicy_containers.pb.h"

// Helper methods for Samba Active Directory authentication, machine (device)
// joining and policy fetching. Note: "Device" and "machine" can be used
// interchangably here.

namespace authpolicy {

class Anonymizer;
class AuthPolicyMetrics;
class PathService;
class ProcessExecutor;

class SambaInterface {
 public:
  SambaInterface(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                 AuthPolicyMetrics* metrics,
                 const PathService* path_service,
                 const base::Closure& user_kerberos_files_changed);

  ~SambaInterface();

  // Creates directories required by Samba code and loads configuration, if it
  // exists. Also loads the debug flags file. Returns error
  // - if a directory failed to create or
  // - if |expect_config| is true and the config file fails to load.
  ErrorType Initialize(bool expect_config);

  // Cleans all persistent state files. Returns true if all files were cleared.
  static bool CleanState(const PathService* path_service);

  // Calls kinit to get a Kerberos ticket-granting-ticket (TGT) for the given
  // |user_principal_name| (format: user_name@workgroup.domain). If a TGT
  // already exists, it is renewed. The password must be readable from the pipe
  // referenced by the file descriptor |password_fd|. On success, the user's
  // account information is returned in |account_info|. If |account_id| is
  // non-empty, the |account_info| is queried by |account_id| instead of by
  // user name. This is safer since the account id is invariant, whereas the
  // user name can change. The updated user name (or rather the sAMAccountName)
  // is returned in the |account_info|. Thus, |account_id| should be set if
  // known and left empty if unknown.
  ErrorType AuthenticateUser(const std::string& user_principal_name,
                             const std::string& account_id,
                             int password_fd,
                             ActiveDirectoryAccountInfo* account_info);

  // Retrieves the status of the user account given by |account_id| (aka
  // objectGUID). The status contains general ActiveDirectoryAccountInfo as well
  // as the status of the user's ticket-granting-ticket (TGT). Does not fill
  // |user_status| on error.
  ErrorType GetUserStatus(const std::string& account_id,
                          ActiveDirectoryUserStatus* user_status);

  // Gets the user Kerberos credential cache (krb5cc) and configuration
  // (krb5.conf) files if they exist. Does not set |files| on error.
  ErrorType GetUserKerberosFiles(const std::string& account_id,
                                 KerberosFiles* files);

  // Joins the local device with name |machine_name| to an Active Directory
  // domain. A user principal name and password are required for authentication
  // (see |AuthenticateUser| for details).
  ErrorType JoinMachine(const std::string& machine_name,
                        const std::string& user_principal_name,
                        int password_fd);

  // Downloads user and extension policy from the Active Directory server and
  // stores it in |gpo_policy_data|. |account_id_key| is the unique user GUID
  // returned from |AuthenticateUser| in |account_info|, prefixed by "a-". The
  // user's Kerberos authentication ticket must still be valid. If this
  // operation fails, call |AuthenticateUser| and try again.
  ErrorType FetchUserGpos(const std::string& account_id_key,
                          protos::GpoPolicyData* gpo_policy_data);

  // Downloads device and extension policy from the Active Directory server and
  // stores it in |gpo_policy_data|. The device must be joined to the Active
  // Directory domain already (see |JoinMachine|). During join, a machine
  // password is stored in a keytab file, which is used for authentication for
  // policy fetch.
  ErrorType FetchDeviceGpos(protos::GpoPolicyData* gpo_policy_data);

  // Sets the default log level, see AuthPolicyFlags::DefaultLevel for details.
  // The level persists between restarts of authpolicyd, but gets reset on
  // reboot.
  void SetDefaultLogLevel(AuthPolicyFlags::DefaultLevel level);

  // Disable retry sleep for unit tests.
  void DisableRetrySleepForTesting() {
    smbclient_retry_sleep_enabled_ = false;
    device_tgt_manager_.DisableRetrySleepForTesting();
  }

  // Returns the anonymizer.
  const Anonymizer* GetAnonymizerForTesting() const {
    return anonymizer_.get();
  }

  // Renew the user ticket-granting-ticket.
  ErrorType RenewUserTgtForTesting() { return user_tgt_manager_.RenewTgt(); }

  const std::string& user_account_id() const { return user_account_id_; }
  const std::string& user_sam_account_name() const {
    return user_sam_account_name_;
  }
  const std::string& machine_name() const { return config_->machine_name(); }

 private:
  // Actual implementation of AuthenticateUser() (see above). The method is
  // wrapped in order to catch and memorize the returned error.
  ErrorType AuthenticateUserInternal(const std::string& user_principal_name,
                                     const std::string& account_id,
                                     int password_fd,
                                     ActiveDirectoryAccountInfo* account_info);

  // Retrieves the name of the domain controller (DC) and the IP of the key
  // distribution center (KDC). If the full server name is 'server.realm', the
  // DC name is set to 'server'. The DC name is required for proper kerberized
  // authentication. The KDC address is required to speed up network
  // communication and get rid of waiting for the machine account propagation
  // after Active Directory domain join.
  ErrorType GetRealmInfo(protos::RealmInfo* realm_info) const;

  // Gets the status of the user's ticket-granting-ticket (TGT). Uses klist
  // internally to check whether the ticket is valid, expired or not present.
  // Does not perform any server-side checks.
  ErrorType GetUserTgtStatus(ActiveDirectoryUserStatus::TgtStatus* tgt_status);

  // Determines the password status by comparing the old |user_pwd_last_set_|
  // timestamp to the new timestamp in |account_info|.
  ActiveDirectoryUserStatus::PasswordStatus GetUserPasswordStatus(
      const ActiveDirectoryAccountInfo& account_info);

  // Retrieves the name of the workgroup. Since the workgroup is expected to
  // change very rarely, this function earlies out and returns ERROR_NONE if the
  // workgroup has already been fetched.
  ErrorType EnsureWorkgroup();

  // Writes the Samba configuration file.
  ErrorType WriteSmbConf() const;

  // Writes the Samba configuration file. If |workgroup_| is empty, the
  // workgroup is queried from the server and the string is updated. Workgroups
  // are only queried once a session, they are expected to change very rarely.
  ErrorType EnsureWorkgroupAndWriteSmbConf();

  // Writes the file with configuration information.
  ErrorType WriteConfiguration() const;

  // Reads the file with configuration information.
  ErrorType ReadConfiguration();

  // Copies the machine keytab file to the state directory. The copy is owned by
  // authpolicyd, so that authpolicyd_exec cannot modify it anymore.
  ErrorType SecureMachineKeyTab() const;

  // Gets user account info. If |account_id| is not empty, searches by
  // objectGUID = |account_id| only. Otherwise, searches by sAMAccountName =
  // |user_name| and - if that fails - by userPrincipalName = |normalized_upn|.
  // Note that sAMAccountName can be different from the name-part of the
  // userPrincipalName and that kinit/Windows prefer sAMAccountName over
  // userPrincipalName. Refreshes the device TGT.
  ErrorType GetAccountInfo(const std::string& user_name,
                           const std::string& normalized_upn,
                           const std::string& account_id,
                           const protos::RealmInfo& realm_info,
                           ActiveDirectoryAccountInfo* account_info);

  // Calls net ads search with given |search_string| to retrieve |account_info|.
  // Authenticates with the device TGT.
  ErrorType SearchAccountInfo(const std::string& search_string,
                              ActiveDirectoryAccountInfo* account_info);

  // Calls net ads gpo list to retrieve a list of GPOs. |user_or_machine_name|
  // may be a user or machine sAMAccountName. (The machine sAMAccountName is the
  // machine name postfixed with '$').
  ErrorType GetGpoList(const std::string& user_or_machine_name,
                       PolicyScope scope,
                       protos::GpoList* gpo_list) const;

  // Downloads user or device GPOs in the given |gpo_list|. |scope| determines
  // a sub-folder where GPOs are downloaded to. It should match |scope| from
  // |GetGpoList|.
  ErrorType DownloadGpos(const protos::GpoList& gpo_list,
                         const std::string& domain_controller_name,
                         PolicyScope scope,
                         std::vector<base::FilePath>* gpo_file_paths) const;

  // Parses GPOs and stores them in user/device policy protobufs.
  ErrorType ParseGposIntoProtobuf(
      const std::vector<base::FilePath>& gpo_file_paths,
      const char* parser_cmd_string,
      std::string* policy_blob) const;

  // Sets and fixes the current user by account id key. Only one account id is
  // allowed per user. Calling this multiple times with different account ids
  // crashes the daemon.
  void SetUser(const std::string& account_id_key);

  // Anonymizes |realm| in different capitalizations as well as all parts. For
  // instance, if realm is SOME.EXAMPLE.COM, anonymizes SOME, EXAMPLE and COM.
  void AnonymizeRealm(const std::string& realm);

  // Resets internal state to an 'unenrolled' state by wiping configuration and
  // user data.
  void Reset();

  // Loads |flags_default_level_| from Path::FLAGS_DEFAULT_LEVEL. Logs an
  // error if the file exists, but the level cannot be loaded. Fails silently if
  // the file does not exist.
  void LoadFlagsDefaultLevel();

  // Saves |flags_default_level_| to Path::FLAGS_DEFAULT_LEVEL. Logs on error.
  void SaveFlagsDefaultLevel();

  // Reloads debug flags. Should be done on every public method called from
  // D-Bus, so that authpolicyd doesn't have to be restarted if the flags
  // change. Note that this is cheap in a production environment where the flags
  // file does not exist, so this is no performance concern.
  void ReloadDebugFlags();

  // User account_id (aka objectGUID).
  std::string user_account_id_;
  // TODO(ljusten): Should contain username with realm after crbug.com/765411 is
  // fixed.
  // User logon name.
  std::string user_sam_account_name_;
  // Timestamp of last password change on server.
  uint64_t user_pwd_last_set_ = 0;
  // Is the user logged in?
  bool user_logged_in_ = false;
  // Last AuthenticateUser() error.
  ErrorType last_auth_error_ = ERROR_NONE;

  std::unique_ptr<protos::ActiveDirectoryConfig> config_;
  std::string workgroup_;

  // The order of members is carefully chosen to match initialization order, so
  // don't mess with it unless you have a reason.

  // UMA statistics, not owned.
  AuthPolicyMetrics* metrics_;

  // Lookup for file paths, not owned.
  const PathService* paths_;

  // Removes sensitive data from logs.
  std::unique_ptr<Anonymizer> anonymizer_;

  // Debug flags, loaded from Path::DEBUG_FLAGS.
  protos::DebugFlags flags_;
  AuthPolicyFlags::DefaultLevel flags_default_level_ = AuthPolicyFlags::kQuiet;

  // Helper to setup and run minijailed processes.
  JailHelper jail_helper_;

  // User and device ticket-granting-ticket managers.
  TgtManager user_tgt_manager_;
  TgtManager device_tgt_manager_;

  // Whether kinit calls may return false negatives and must be retried.
  bool retry_machine_kinit_ = false;

  // Whether to sleep when retrying smbclient (disable for testing).
  bool smbclient_retry_sleep_enabled_ = true;

  DISALLOW_COPY_AND_ASSIGN(SambaInterface);
};

}  // namespace authpolicy

#endif  // AUTHPOLICY_SAMBA_INTERFACE_H_
