// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/username_passkey.h"

#include <brillo/secure_blob.h>

#include "cryptohome/crypto.h"
#include "cryptohome/obfuscated_username.h"

namespace cryptohome {
using brillo::SecureBlob;

UsernamePasskey::UsernamePasskey(const char *username,
                                 const brillo::SecureBlob& passkey)
    : username_(username, strlen(username)),
      passkey_() {
  passkey_.assign(passkey.begin(), passkey.end());
}

UsernamePasskey::~UsernamePasskey() {
}

void UsernamePasskey::Assign(const UsernamePasskey& rhs) {
  username_.assign(rhs.username());
  key_data_ = rhs.key_data();
  SecureBlob passkey;
  rhs.GetPasskey(&passkey);
  passkey_.swap(passkey);
  challenge_credentials_keyset_info_ = rhs.challenge_credentials_keyset_info_;
}

void UsernamePasskey::set_key_data(const KeyData& data) {
  key_data_ = data;
}

void UsernamePasskey::set_challenge_credentials_keyset_info(
    const SerializedVaultKeyset_SignatureChallengeInfo&
        challenge_credentials_keyset_info) {
  challenge_credentials_keyset_info_ = challenge_credentials_keyset_info;
}

const KeyData& UsernamePasskey::key_data() const {
  return key_data_;
}

const SerializedVaultKeyset_SignatureChallengeInfo&
UsernamePasskey::challenge_credentials_keyset_info() const {
  return challenge_credentials_keyset_info_;
}

std::string UsernamePasskey::username() const {
  return username_;
}

std::string UsernamePasskey::GetObfuscatedUsername(
    const brillo::SecureBlob &system_salt) const {
  return BuildObfuscatedUsername(username_, system_salt);
}

void UsernamePasskey::GetPasskey(SecureBlob* passkey) const {
  *passkey = passkey_;
}

}  // namespace cryptohome
