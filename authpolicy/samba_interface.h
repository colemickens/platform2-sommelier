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
#include <dbus/authpolicy/dbus-constants.h>

#include "bindings/authpolicy_containers.pb.h"

// Helper methods for samba Active Directory authentication, machine (device)
// joining and policy fetching. Note: "Device" and "machine" can be used
// interchangably here.

namespace authpolicy {

class SambaInterface {
 public:
  // Creates directories required by Samba code and loads configuration, if it
  // exists. Returns false
  // - if a directory failed to create or
  // - if |expect_config| is true and the config file fails to load.
  bool Initialize(bool expect_config);

  // Calls kinit to get a Kerberos ticket-granting-ticket (TGT) for the given
  // |user_principal_name| (format: user_name@workgroup.domain). If a TGT
  // already exists, it is renewed. The password must be readable from the pipe
  // referenced by the file descriptor |password_fd|. On success, the user's
  // object GUID is returned in |out_account_id|. The GUID uniquely identifies
  // the user's account.
  bool AuthenticateUser(const std::string& user_principal_name,
                        int password_fd,
                        std::string* out_account_id,
                        ErrorType* out_error);

  // Joins the local device with name |machine_name| to an Active Directory
  // domain. A user principal name and password are required for authentication
  // (see |AuthenticateUser| for details).
  bool JoinMachine(const std::string& machine_name,
                   const std::string& user_principal_name,
                   int password_fd,
                   ErrorType* out_error);

  // Downloads user policy from the Active Directory server and stores it in a
  // serialized user policy protobuf string (see |CloudPolicySettings|).
  // |account_id| is the unique user GUID returned from |AuthenticateUser|. The
  // user's Kerberos authentication ticket must still be valid. If this
  // operation fails, call |AuthenticateUser| and try again.
  bool FetchUserGpos(const std::string& account_id,
                     std::string* out_policy_blob,
                     ErrorType* out_error);

  // Downloads device policy from the Active Directory server and stores it in a
  // serialized device policy protobuf string (see |ChromeDeviceSettingsProto|).
  // The device must be joined to the Active Directory domain already (see
  // |JoinMachine|). During join, a machine password is stored in a keytab file,
  // which is used for authentication for policy fetch.
  bool FetchDeviceGpos(std::string* out_policy_blob, ErrorType* out_error);

 private:
  // Cached state
  std::unordered_map<std::string, std::string> account_id_key_user_name_map_;
  std::unique_ptr<protos::SambaConfig> config_;
  std::string domain_controller_name_;
  std::string workgroup_;

  // Whether kinit calls may return false negatives and must be retried.
  bool retry_machine_kinit_ = false;
};

}  // namespace authpolicy

#endif  // AUTHPOLICY_SAMBA_INTERFACE_H_
