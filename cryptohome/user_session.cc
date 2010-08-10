// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains the implementation of class Mount

#include "user_session.h"

#include <openssl/evp.h>

#include "crypto.h"

namespace cryptohome {

const int kUserSessionSaltLength = 16;
const int kUserSessionIdLength = 128;

UserSession::UserSession()
    : username_(),
      crypto_(NULL),
      username_salt_(),
      key_salt_(),
      cipher_() {
}

UserSession::~UserSession() {
}

void UserSession::Init(Crypto* crypto) {
  crypto_ = crypto;
}

bool UserSession::SetUser(const Credentials& credentials) {
  username_salt_.resize(kUserSessionSaltLength);
  crypto_->GetSecureRandom(static_cast<unsigned char*>(username_salt_.data()),
                           username_salt_.size());
  username_ = credentials.GetObfuscatedUsername(username_salt_);

  key_salt_.resize(PKCS5_SALT_LEN);
  crypto_->GetSecureRandom(static_cast<unsigned char*>(key_salt_.data()),
                           key_salt_.size());
  SecureBlob plaintext(kUserSessionIdLength);
  crypto_->GetSecureRandom(static_cast<unsigned char*>(plaintext.data()),
                           plaintext.size());

  SecureBlob passkey;
  credentials.GetPasskey(&passkey);

  SecureBlob aes_key;
  SecureBlob aes_iv;
  if (!crypto_->PasskeyToAesKey(passkey,
                                key_salt_,
                                cryptohome::kDefaultPasswordRounds,
                                &aes_key,
                                &aes_iv)) {
    return false;
  }

  return crypto_->WrapAes(plaintext, 0, plaintext.size(), aes_key, aes_iv,
                          Crypto::PADDING_CRYPTOHOME_DEFAULT, &cipher_);
}

void UserSession::Reset() {
  username_ = "";
  username_salt_.resize(0);
  key_salt_.resize(0);
  cipher_.resize(0);
}

bool UserSession::CheckUser(const Credentials& credentials) const {
  std::string username = credentials.GetObfuscatedUsername(username_salt_);

  return (username.compare(username_) == 0);
}

bool UserSession::Verify(const Credentials& credentials) const {
  if (!CheckUser(credentials)) {
    return false;
  }

  SecureBlob passkey;
  credentials.GetPasskey(&passkey);

  SecureBlob aes_key;
  SecureBlob aes_iv;
  if (!crypto_->PasskeyToAesKey(passkey,
                                key_salt_,
                                cryptohome::kDefaultPasswordRounds,
                                &aes_key,
                                &aes_iv)) {
    return false;
  }

  SecureBlob plaintext;
  return crypto_->UnwrapAes(cipher_, 0, cipher_.size(), aes_key, aes_iv,
                            Crypto::PADDING_CRYPTOHOME_DEFAULT, &plaintext);
}

}  // namespace cryptohome
