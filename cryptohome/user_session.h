// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// UserSession - the UserSession class is used in re-authenticating the
// currently logged-in user.  It allows offline credentials verification
// post-login without the expense of a TPM crypto operation (when the TPM is
// used for added security).  User session works by generating a random blob and
// encrypting it using the user's credentials at login.  When an offline
// credentials check occurs for this user, UserSession attempts to decrypt the
// encrypted representation of that blob.  A successful decryption means that
// the supplied credentials are correct.

#ifndef CRYPTOHOME_USER_SESSION_H_
#define CRYPTOHOME_USER_SESSION_H_

#include <base/basictypes.h>

#include "credentials.h"
#include "secure_blob.h"

namespace cryptohome {

class Crypto;

class UserSession {
 public:
  UserSession();
  virtual ~UserSession();

  // Initializes the UserSession object.
  //
  // Parameters
  //   crypto - The crypto context to use for this session
  //   salt - The salt to use for the username
  virtual void Init(Crypto* crypto, const SecureBlob& salt);

  // Assigns a user to the UserSession object.  The random blob is created and
  // encrypted with the supplied credentials.
  //
  // Parameters
  //   username - The user credentials to initialize this session with
  virtual bool SetUser(const Credentials& username);

  // Resets the UserSession, clearing the current user and cipher text used for
  // verification.
  virtual void Reset();

  // Checks that the user supplied is the user associated with this session
  //
  // Parameters
  //   username - The user to check this session against
  virtual bool CheckUser(const Credentials& username) const;

  // Checks that the user's credentials successfully decrypt the ciphertext
  // associated with this session (and are therefore valid for this user).
  //
  // Parameters
  //   credentials - The user credentials to attempt decryption with
  virtual bool Verify(const Credentials& credentials) const;

  // Get the obfuscated username of this session
  //
  // Parameters
  //   username (OUT) - the username
  virtual void GetObfuscatedUsername(std::string* username) const;

  // Get the plaintext username of this session
  //
  // Parameters
  //   username (OUT) - the username
  virtual void GetUsername(std::string* username) const;

  // Push a mount onto the stack of mounts for this user's session
  //
  // Parameters
  //   mount_point - Mount point of this mount
  virtual void PushMount(const std::string& mount_point);

  // Pops a mount from the stack of mounts for this user's session
  // Returns false if there are no remaining mounts
  //
  // Parameters
  //   mount_point (OUT) - Mount point popped from the stack
  virtual bool PopMount(std::string* mount_point);

 private:
  // Logs if there are any mounts still remaining.
  virtual void LogIfAnyMounts();

  std::string obfuscated_username_;
  std::string username_;
  Crypto* crypto_;
  SecureBlob username_salt_;
  SecureBlob key_salt_;
  SecureBlob cipher_;
  std::vector<std::string> mount_points_;

  DISALLOW_COPY_AND_ASSIGN(UserSession);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_USER_SESSION_H_
