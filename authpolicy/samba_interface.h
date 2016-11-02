// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AUTHPOLICY_SAMBA_INTERFACE_H_
#define AUTHPOLICY_SAMBA_INTERFACE_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "base/files/file_path.h"

// Helper methods for samba AD authentication, machine (device) joining and
// policy fetching. Note: "Device" and "machine" can be used interchangably
// here.

namespace authpolicy {

class SambaInterface {
 public:
  // STUB CODE, TO BE FILLED.

  // Calls kinit to get a Kerberos ticket-granting-ticket (TGT) for the given
  // |user_principal_name| (format: user_name@workgroup.domain). If a TGT
  // already exists, it is renewed. The password must be readable from the pipe
  // referenced by the file descriptor |password_fd|. On success, the user's
  // object GUID is returned in |out_account_id|. The GUID uniquely identifies
  // the user's account.
  bool AuthenticateUser(const std::string& user_principal_name, int password_fd,
                        std::string* out_account_id,
                        const char** out_error_code) {
    return false;
  }

  // Joins the local device with name |machine_name| to an AD domain. A user
  // principal name and password are required for authentication (see
  // |AuthenticateUser| for details).
  bool JoinMachine(const std::string& machine_name,
                   const std::string& user_principal_name, int password_fd,
                   const char** out_error_code) {
    return false;
  }

  // Downloads user policy files from the AD server. |account_id| is the unique
  // user GUID returned from |AuthenticateUser|. The user's Kerberos
  // authentication ticket must still be valid. If this operation fails, call
  // |AuthenticateUser| and try again. User policy is given as Registry.pol
  // (preg) files, a binary format encoding policy data.
  bool FetchUserGpos(const std::string& account_id,
                     std::vector<base::FilePath>* out_gpo_file_paths,
                     const char** out_error_code) {
    return false;
  }

  // Downloads device policy files from the AD server. The device must be joined
  // to the AD domain already (see |JoinMachine|). During join, a machine
  // password is stored in a keytab file, which is used for authentication for
  // policy fetch. Device policy is given as Registry.pol (preg) files, a binary
  // format encoding policy data.
  bool FetchDeviceGpos(std::vector<base::FilePath>* out_gpo_file_paths,
                       const char** out_error_code) {
    return false;
  }
};

}  // namespace authpolicy

#endif  // AUTHPOLICY_SAMBA_INTERFACE_H_
