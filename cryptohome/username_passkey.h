// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// UsernamePasskey wraps a username/passkey pair that can be used to
// authenticate a user.

#ifndef CRYPTOHOME_USERNAME_PASSKEY_H_
#define CRYPTOHOME_USERNAME_PASSKEY_H_

#include "credentials.h"

#include <base/basictypes.h>
#include <chromeos/secure_blob.h>
#include <string>

namespace cryptohome {

class UsernamePasskey : public Credentials {
 public:
  UsernamePasskey() { }

  // Constructs UsernamePasskey from username strings and passkeys and passwords
  UsernamePasskey(const char* username, const chromeos::Blob& passkey);

  ~UsernamePasskey();

  void Assign(const Credentials& rhs);

  // Overridden from cryptohome::Credentials
  std::string username() const;
  std::string GetObfuscatedUsername(const chromeos::Blob &system_salt) const;
  void GetPasskey(chromeos::SecureBlob* passkey) const;

 private:
  std::string username_;
  chromeos::SecureBlob passkey_;

  DISALLOW_COPY_AND_ASSIGN(UsernamePasskey);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_USERNAME_PASSKEY_H_
