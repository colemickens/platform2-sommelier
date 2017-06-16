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

#include "authpolicy/anonymizer.h"
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

class AuthPolicyMetrics;
class PathService;
class ProcessExecutor;

class SambaInterface {
 public:
  SambaInterface(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                 AuthPolicyMetrics* metrics,
                 const PathService* path_service);

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

  // Joins the local device with name |machine_name| to an Active Directory
  // domain. A user principal name and password are required for authentication
  // (see |AuthenticateUser| for details).
  ErrorType JoinMachine(const std::string& machine_name,
                        const std::string& user_principal_name,
                        int password_fd);

  // Downloads user policy from the Active Directory server and stores it in a
  // serialized user policy protobuf string (see |CloudPolicySettings|).
  // |account_id| is the unique user GUID returned from |AuthenticateUser|. The
  // user's Kerberos authentication ticket must still be valid. If this
  // operation fails, call |AuthenticateUser| and try again.
  ErrorType FetchUserGpos(const std::string& account_id,
                          std::string* policy_blob);

  // Downloads device policy from the Active Directory server and stores it in a
  // serialized device policy protobuf string (see |ChromeDeviceSettingsProto|).
  // The device must be joined to the Active Directory domain already (see
  // |JoinMachine|). During join, a machine password is stored in a keytab file,
  // which is used for authentication for policy fetch.
  ErrorType FetchDeviceGpos(std::string* policy_blob);

  // Sets the default log level, see AuthPolicyFlags::DefaultLevel for details.
  // The level persists between restarts of authpolicyd, but gets reset on
  // reboot.
  void SetDefaultLogLevel(AuthPolicyFlags::DefaultLevel level);

  // Disable retry sleep for unit tests.
  void DisableRetrySleepForTesting() {
    smbclient_retry_sleep_enabled_ = false;
    device_tgt_manager_.DisableRetrySleepForTesting();
  }

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

  // Determines the password status by comparing the store last change timestamp
  // to the timestamp we just got from the server. |prev_pw_last_set| is the
  // timestamp of the last password change stored in UserData.
  ActiveDirectoryUserStatus::PasswordStatus GetUserPasswordStatus(
      const ActiveDirectoryAccountInfo& account_info,
      uint64_t* prev_pw_last_set);

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

  // Data we memorize for each user.
  struct UserData {
    std::string sam_account_name_;  // Logon name.
    uint64_t pwd_last_set_ = 0;  // Timestamp of last password change on server.
    ErrorType last_auth_error_ = ERROR_NONE;  // Last AuthenticateUser() error.
    UserData() = default;
    UserData(const std::string& sam_account_name, uint64_t pwd_last_set)
        : sam_account_name_(sam_account_name), pwd_last_set_(pwd_last_set) {}
  };

  // Maps the account id key ("a-" + user object GUID) to user data.
  std::unordered_map<std::string, UserData> user_data_;
  std::unique_ptr<protos::ActiveDirectoryConfig> config_;
  std::string workgroup_;

  // The order of members is carefully chosen to match initialization order, so
  // don't mess with it unless you have a reason.

  // UMA statistics, not owned.
  AuthPolicyMetrics* metrics_;

  // Lookup for file paths, not owned.
  const PathService* paths_;

  // Debug flags, loaded from Path::DEBUG_FLAGS.
  protos::DebugFlags flags_;
  AuthPolicyFlags::DefaultLevel flags_default_level_ = AuthPolicyFlags::kQuiet;

  // Helper to setup and run minijailed processes.
  JailHelper jail_helper_;

  // User and device ticket-granting-ticket managers.
  TgtManager user_tgt_manager_;
  TgtManager device_tgt_manager_;

  // Removes sensitive data from logs.
  Anonymizer anonymizer_;

  // Whether kinit calls may return false negatives and must be retried.
  bool retry_machine_kinit_ = false;

  // Whether to sleep when retrying smbclient (disable for testing).
  bool smbclient_retry_sleep_enabled_ = true;

  DISALLOW_COPY_AND_ASSIGN(SambaInterface);
};

}  // namespace authpolicy

#endif  // AUTHPOLICY_SAMBA_INTERFACE_H_
