// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// UsernamePasskey wraps a username/passkey pair that can be used to
// authenticate a user.

#ifndef CRYPTOHOME_USERNAME_PASSKEY_H_
#define CRYPTOHOME_USERNAME_PASSKEY_H_

#include "cryptohome/credentials.h"

#include <string>

#include <base/macros.h>
#include <brillo/secure_blob.h>

#include "key.pb.h"           // NOLINT(build/include)
#include "vault_keyset.pb.h"  // NOLINT(build/include)

namespace cryptohome {

class UsernamePasskey : public Credentials {
 public:
  UsernamePasskey() { }

  // Constructs UsernamePasskey from username strings and passkeys and passwords
  UsernamePasskey(const char* username, const brillo::SecureBlob& passkey);

  ~UsernamePasskey();

  void Assign(const UsernamePasskey& rhs);

  void set_key_data(const KeyData& data);
  void set_challenge_credentials_keyset_info(
      const SerializedVaultKeyset_SignatureChallengeInfo&
          challenge_credentials_keyset_info);

  // Overridden from cryptohome::Credentials
  std::string username() const;
  const KeyData& key_data() const;
  const SerializedVaultKeyset_SignatureChallengeInfo&
  challenge_credentials_keyset_info() const;
  std::string GetObfuscatedUsername(
      const brillo::SecureBlob &system_salt) const;
  void GetPasskey(brillo::SecureBlob* passkey) const;

 private:
  std::string username_;
  KeyData key_data_;
  SerializedVaultKeyset_SignatureChallengeInfo
      challenge_credentials_keyset_info_;
  brillo::SecureBlob passkey_;

  DISALLOW_COPY_AND_ASSIGN(UsernamePasskey);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_USERNAME_PASSKEY_H_
