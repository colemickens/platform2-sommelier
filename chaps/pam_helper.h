// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A helper which provides methods to extract data from a pam_handle.

#ifndef CHAPS_PAM_HELPER_H_
#define CHAPS_PAM_HELPER_H_

#include <security/pam_appl.h>
#include <security/pam_modules.h>

#include <string>

#include <brillo/secure_blob.h>

namespace chaps {

class PamHelper {
 public:
  virtual ~PamHelper();

  // Gets the name of the user that is logging in for the current PAM session.
  //  pam_handle: The PAM handle associated with the current session.
  //  user: Returns the name of the user.
  virtual bool GetPamUser(pam_handle_t* pam_handle, std::string* user);

  // Gets the password provided by the user to authenticate their current PAM
  // session.
  //  pam_handle: The PAM handle associated with the current session.
  //  old_password: If true, PAM_OLDAUTHTOK will be retrieved instead of
  //                PAM_AUTHTOK, thus retrieving the old password if available.
  //  data: Returns the users password.
  virtual bool GetPamPassword(pam_handle_t* pam_handle,
                              bool old_password,
                              brillo::SecureBlob* data);

  // Saves the username and password in the pam_handle such that it can be
  // retrieved by RetrieveUserAndPassword() at a later point.
  //  pam_handle: The PAM handle associated with the current session.
  //  user: User name to save.
  //  password: Password to save.
  virtual bool SaveUserAndPassword(pam_handle_t* pam_handle,
                                   const std::string& user,
                                   const brillo::SecureBlob& password);

  // Retrieves the username and password previously saved in the pam_handle.
  // Returns true on success.
  //  pam_handle: The PAM handle associated with the current session.
  //  user: Returns the saved user name.
  //  password: Returns the saved password.
  virtual bool RetrieveUserAndPassword(pam_handle_t* pam_handle,
                                       std::string* user,
                                       brillo::SecureBlob* password);

  // Updates the PAM environment to add an environment variable with the given
  // value.
  //  pam_handle: The PAM handle associated with the current session.
  //  name: The name of the environment variable to put.
  //  value: The value to set the environment variable to.
  virtual bool PutEnvironmentVariable(pam_handle_t* pam_handle,
                                      const std::string& name,
                                      const std::string& value);

  // Gets the value of the given environment variable from the PAM environment.
  //  pam_handle: The PAM handle associated with the current session.
  //  name: The name of the environment variable to get.
  //  value: Returns the value of the environment variable.
  virtual bool GetEnvironmentVariable(pam_handle_t* pam_handle,
                                      const std::string& name,
                                      std::string* value);
};

}  // namespace chaps

#endif  // CHAPS_PAM_HELPER_H_
