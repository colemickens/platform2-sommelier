// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// UsernamePasskey wraps a username/passkey pair that can be used to
// authenticate a user.

#ifndef CRYPTOHOME_USERNAME_PASSKEY_H_
#define CRYPTOHOME_USERNAME_PASSKEY_H_

#include "credentials.h"

#include <base/basictypes.h>
#include <string>

namespace cryptohome {

class UsernamePasskey : public Credentials {
 public:
  // Constructs UsernamePasskey from username strings and passkeys and passwords
  UsernamePasskey(const char* username, const chromeos::Blob& passkey);

  ~UsernamePasskey();

  // Overridden from cryptohome::Credentials
  void GetFullUsername(char *name_buffer, int length) const;
  std::string GetFullUsernameString() const;
  void GetPartialUsername(char *name_buffer, int length) const;
  std::string GetObfuscatedUsername(const chromeos::Blob &system_salt) const;
  void GetPasskey(SecureBlob* passkey) const;

 private:
  std::string username_;
  SecureBlob passkey_;

  DISALLOW_COPY_AND_ASSIGN(UsernamePasskey);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_USERNAME_PASSKEY_H_
