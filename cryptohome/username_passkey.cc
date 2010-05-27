// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/username_passkey.h"

#include <openssl/sha.h>

#include "base/logging.h"
#include "chromeos/utility.h"

namespace cryptohome {
using std::string;

UsernamePasskey::UsernamePasskey(const char *username,
                                   const int username_length,
                                   const char *passkey,
                                   const int passkey_length)
    : username_(username, username_length),
      passkey_(passkey_length) {
  memcpy(&passkey_[0], passkey, passkey_length);
}

UsernamePasskey::UsernamePasskey(const char *username,
                                   const int username_length,
                                   const chromeos::Blob& passkey)
    : username_(username, username_length),
      passkey_() {
  passkey_.assign(passkey.begin(), passkey.end());
}

UsernamePasskey::UsernamePasskey(const char *username,
                                   const int username_length,
                                   const std::string& passkey)
    : username_(username, username_length),
      passkey_() {
  passkey_.resize(passkey.length());
  memcpy(&passkey_[0], passkey.c_str(), passkey.length());
}

UsernamePasskey::UsernamePasskey(const UsernamePasskey& rhs)
    : username_(rhs.username_),
      passkey_(rhs.passkey_) {
}

UsernamePasskey::~UsernamePasskey() {
}

UsernamePasskey& UsernamePasskey::operator=(const UsernamePasskey& rhs) {
  this->username_ = rhs.username_;
  this->passkey_ = rhs.passkey_;
  return *this;
}

UsernamePasskey UsernamePasskey::FromUsernamePassword(const char* username,
    const char* password,
    const chromeos::Blob& salt) {
  SecureBlob passkey = PasswordToPasskey(password, salt);
  return UsernamePasskey(username, strlen(username), passkey);
}

SecureBlob UsernamePasskey::PasswordToPasskey(const char *password,
                                              const chromeos::Blob& salt) {
  CHECK(password);

  std::string ascii_salt = chromeos::AsciiEncode(salt);
  // Convert a raw password to a password hash
  SHA256_CTX sha_ctx;
  unsigned char md_value[SHA256_DIGEST_LENGTH];

  SHA256_Init(&sha_ctx);
  SHA256_Update(&sha_ctx,
                reinterpret_cast<const unsigned char*>(ascii_salt.data()),
                static_cast<unsigned int>(ascii_salt.length()));
  SHA256_Update(&sha_ctx, password, strlen(password));
  SHA256_Final(md_value, &sha_ctx);

  SecureBlob password_hash(SHA256_DIGEST_LENGTH);
  AsciiEncodeToBuffer(md_value, SHA256_DIGEST_LENGTH / 2,
                      reinterpret_cast<char*>(&password_hash[0]),
                      password_hash.size());
  chromeos::SecureMemset(md_value, sizeof(md_value), 0);
  return password_hash;
}

void UsernamePasskey::AsciiEncodeToBuffer(const unsigned char* source,
                                          int source_length, char* buffer,
                                          int buffer_length) {
  const char hex_chars[] = "0123456789abcdef";
  int i = 0;
  for (int index = 0;
       (index < source_length) && ((i + 1) < buffer_length);
       index++) {
    buffer[i++] = hex_chars[(source[index] >> 4) & 0x0f];
    buffer[i++] = hex_chars[source[index] & 0x0f];
  }
  if (i < buffer_length) {
    buffer[i] = '\0';
  }
}

void UsernamePasskey::GetFullUsername(char *name_buffer, int length) const {
  strncpy(name_buffer, username_.c_str(), length);
}

std::string UsernamePasskey::GetFullUsername() const {
  return username_;
}

void UsernamePasskey::GetPartialUsername(char *name_buffer, int length) const {
  size_t at_index = username_.find('@');
  string partial = username_.substr(0, at_index);
  strncpy(name_buffer, partial.c_str(), length);
}

string UsernamePasskey::GetObfuscatedUsername(
    const chromeos::Blob &system_salt) const {
  CHECK(!username_.empty());

  SHA_CTX ctx;
  unsigned char md_value[SHA_DIGEST_LENGTH];

  SHA1_Init(&ctx);
  SHA1_Update(&ctx, &system_salt[0], system_salt.size());
  SHA1_Update(&ctx, username_.c_str(), username_.length());
  SHA1_Final(md_value, &ctx);

  chromeos::Blob md_blob(md_value,
               md_value + (SHA_DIGEST_LENGTH * sizeof(unsigned char)));

  return chromeos::AsciiEncode(md_blob);
}

SecureBlob UsernamePasskey::GetPasskey() const {
  return passkey_;
}

}  // namespace cryptohome
