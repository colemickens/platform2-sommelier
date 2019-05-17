// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains the implementation of class Mount

#include "cryptohome/user_session.h"

#include <string>

#include <openssl/evp.h>

#include <base/logging.h>
#include "cryptohome/cryptolib.h"

using brillo::SecureBlob;

namespace cryptohome {

const int kUserSessionIdLength = 128;

UserSession::UserSession() { }

UserSession::~UserSession() { }

void UserSession::Init(const SecureBlob& salt) {
  username_salt_.assign(salt.begin(), salt.end());
}

bool UserSession::SetUser(const Credentials& credentials) {
  obfuscated_username_ = credentials.GetObfuscatedUsername(username_salt_);
  username_ = credentials.username();
  key_data_ = credentials.key_data();
  key_index_ = -1;  // Invalid key index.

  key_salt_.resize(PKCS5_SALT_LEN);
  CryptoLib::GetSecureRandom(key_salt_.data(), key_salt_.size());
  SecureBlob plaintext(kUserSessionIdLength);
  CryptoLib::GetSecureRandom(plaintext.data(), plaintext.size());

  SecureBlob passkey;
  credentials.GetPasskey(&passkey);

  SecureBlob aes_key;
  SecureBlob aes_iv;
  if (!CryptoLib::PasskeyToAesKey(passkey,
                                  key_salt_,
                                  cryptohome::kDefaultPasswordRounds,
                                  &aes_key,
                                  &aes_iv)) {
    return false;
  }

  return CryptoLib::AesEncrypt(plaintext, aes_key, aes_iv, &cipher_);
}

void UserSession::Reset() {
  username_ = "";
  obfuscated_username_ = "";
  key_salt_.resize(0);
  cipher_.resize(0);
  key_index_ = -1;
  key_data_.Clear();
}

bool UserSession::CheckUser(const Credentials& credentials) const {
  std::string username = credentials.GetObfuscatedUsername(username_salt_);

  return (username.compare(obfuscated_username_) == 0);
}

bool UserSession::Verify(const Credentials& credentials) const {
  if (!CheckUser(credentials)) {
    return false;
  }
  // If the incoming credentials have no label, then just
  // test the secret.  If it is labeled, then the label must
  // match.
  if (!credentials.key_data().label().empty() &&
      credentials.key_data().label() != key_data_.label()) {
    return false;
  }

  SecureBlob passkey;
  credentials.GetPasskey(&passkey);

  SecureBlob aes_key;
  SecureBlob aes_iv;
  if (!CryptoLib::PasskeyToAesKey(passkey,
                                  key_salt_,
                                  cryptohome::kDefaultPasswordRounds,
                                  &aes_key,
                                  &aes_iv)) {
    return false;
  }

  SecureBlob plaintext;
  return CryptoLib::AesDecrypt(cipher_, aes_key, aes_iv, &plaintext);
}

void UserSession::GetObfuscatedUsername(std::string* username) const {
  username->assign(obfuscated_username_);
}

int UserSession::key_index() const {
  LOG_IF(WARNING, key_index_ < 0)
                << "Attempt to access an uninitialized key_index."
                << "Guest mount? Ephemeral mount?";
  return key_index_;
}

}  // namespace cryptohome
