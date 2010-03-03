// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// UsernamePasshash wraps a username/salted password hash pair that
// can be used to authenticate a user.

#ifndef CRYPTOHOME_USERNAME_PASSHASH_H_
#define CRYPTOHOME_USERNAME_PASSHASH_H_

#include "cryptohome/credentials.h"

#include <string.h>

#include "base/basictypes.h"

namespace cryptohome {

class UsernamePasshash : public Credentials {
 public:
  // |passhash| is a weak hash of the user's password, using the same
  // algorithm that pam/pam_google/pam_mount use to get the user's
  // plaintext passhash safely passed on to the login session.  That
  // means we compute a sha256sum of the ASCII encoded system salt plus
  // the plaintext passhash, ASCII encode the result, and take the first
  // 32 bytes.  To say that in bash...
  //
  //    $(cat <(echo -n $(xxd -p "$SYSTEM_SALT_FILE"))
  //      <(echo -n "$PASSHASH") | sha256sum | head -c 32)
  //
  UsernamePasshash(const char *username, const int username_length,
                   const char *passhash, const int passhash_length);

  ~UsernamePasshash() {}

  // Overridden from cryptohome::Credentials
  void GetFullUsername(char *name_buffer, int length) const;
  void GetPartialUsername(char *name_buffer, int length) const;
  std::string GetObfuscatedUsername(const chromeos::Blob &system_salt) const;
  std::string GetPasswordWeakHash(const chromeos::Blob &system_salt) const;

 private:
  std::string username_;
  std::string passhash_;

  DISALLOW_COPY_AND_ASSIGN(UsernamePasshash);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_USERNAME_PASSHASH_H_
