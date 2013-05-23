// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A login manager deals with login events related to Chaps.

#ifndef CHAPS_ISOLATE_LOGIN_CLIENT_H_
#define CHAPS_ISOLATE_LOGIN_CLIENT_H_

#include <string>

#include <base/file_path.h>
#include <chromeos/secure_blob.h>

#include "chaps/isolate.h"
#include "chaps/token_file_manager.h"
#include "chaps/token_manager_client.h"

namespace chaps {

class IsolateLoginClient {
 public:
  // Does not take ownership of arguments.
  IsolateLoginClient(IsolateCredentialManager* isolate_manager,
                     TokenFileManager* file_manager,
                     TokenManagerClient* token_manager);
  virtual ~IsolateLoginClient();

  // Should be called whenever a user logs into a session. Will ensure that
  // chaps has an open isolate for the user and that their token is load into
  // this isolate, thus providing applications running in the users session
  // with access to their TPM protected keys.  Return true on success and
  // false on failure.
  virtual bool LoginUser(const std::string& user,
                         const chromeos::SecureBlob& auth_data);

  // Should be called whenever a user logs out of a session. If the user has
  // logged out of all sessions, this will close their isolate and unload
  // their token.  Return true on success and false on failure.
  virtual bool LogoutUser(const std::string& user);

  // Change the authorization data used to secure the users token.
  // Return true on success and false on failure.
  virtual bool ChangeUserAuth(const std::string& user,
                              const chromeos::SecureBlob& old_auth_data,
                              const chromeos::SecureBlob& new_auth_data);

 private:
  IsolateCredentialManager* isolate_manager_;
  TokenFileManager* file_manager_;
  TokenManagerClient* token_manager_;

  DISALLOW_COPY_AND_ASSIGN(IsolateLoginClient);
};

}  // namespace chaps

#endif  // CHAPS_ISOLATE_LOGIN_CLIENT_H_
