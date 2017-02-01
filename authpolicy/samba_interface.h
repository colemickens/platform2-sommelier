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
#include <dbus/authpolicy/dbus-constants.h>

#include "authpolicy/constants.h"
#include "authpolicy/path_service.h"
#include "bindings/authpolicy_containers.pb.h"

// Helper methods for Samba Active Directory authentication, machine (device)
// joining and policy fetching. Note: "Device" and "machine" can be used
// interchangably here.

namespace authpolicy {

class ProcessExecutor;

class SambaInterface {
 public:
  SambaInterface();

  // Creates directories required by Samba code and loads configuration, if it
  // exists. Also loads the debug flags file. Returns error
  // - if a directory failed to create or
  // - if |expect_config| is true and the config file fails to load.
  ErrorType Initialize(std::unique_ptr<PathService> path_service,
                       bool expect_config);

  // Calls kinit to get a Kerberos ticket-granting-ticket (TGT) for the given
  // |user_principal_name| (format: user_name@workgroup.domain). If a TGT
  // already exists, it is renewed. The password must be readable from the pipe
  // referenced by the file descriptor |password_fd|. On success, the user's
  // object GUID is returned in |account_id|. The GUID uniquely identifies
  // the user's account.
  ErrorType AuthenticateUser(const std::string& user_principal_name,
                             int password_fd,
                             std::string* account_id);

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

 private:
  // Sets up minijail and executes |cmd|. |seccomp_path_key| specifies the path
  // of the seccomp filter to use.
  bool SetupJailAndRun(ProcessExecutor* cmd, Path seccomp_path_key) const;

  // Sets up trace logging for kinit.
  void SetupKinitTrace(ProcessExecutor* kinit_cmd) const;

  // Prints out the trace log of kinit.
  void OutputKinitTrace() const;

  // Retrieves the name of the domain controller (DC) and the IP of the key
  // distribution center (KDC). If the full server name is 'server.realm', the
  // DC name is set to 'server'. The DC name is required for proper kerberized
  // authentication. The KDC address is required to speed up network
  // communication and get rid of waiting for the machine account propagation
  // after Active Directory domain join.
  ErrorType GetRealmInfo(protos::RealmInfo* realm_info) const;

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

  // Writes the krb5 configuration file.
  ErrorType WriteKrb5Conf(const std::string& realm,
                          const std::string& kdc_ip) const;

  // Writes the file with configuration information.
  ErrorType WriteConfiguration() const;

  // Reads the file with configuration information.
  ErrorType ReadConfiguration();

  // Copies the machine keytab file to the state directory. The copy is owned by
  // authpolicyd, so that authpolicyd_exec cannot modify it anymore.
  ErrorType SecureMachineKeyTab() const;

  // Calls net ads search with given |search_string| to retrieve account info.
  ErrorType GetAccountInfo(const std::string& search_string,
                           protos::AccountInfo* account_info) const;

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

  // Maps the account id key ("a-" + user object GUID) to sAMAccountName.
  std::unordered_map<std::string, std::string> user_id_name_map_;
  std::unique_ptr<protos::ActiveDirectoryConfig> config_;
  std::string workgroup_;

  // Whether kinit calls may return false negatives and must be retried.
  bool retry_machine_kinit_ = false;

  // Debug flags.
  bool disable_seccomp_filters_ = false;
  bool log_seccomp_filters_ = false;
  bool trace_kinit_ = false;

  // User ids.
  uid_t authpolicyd_uid_ = -1;
  uid_t authpolicyd_exec_uid_ = -1;

  // Lookup for file paths.
  std::unique_ptr<PathService> paths_;

  DISALLOW_COPY_AND_ASSIGN(SambaInterface);
};

}  // namespace authpolicy

#endif  // AUTHPOLICY_SAMBA_INTERFACE_H_
