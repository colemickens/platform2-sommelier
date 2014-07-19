// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/username_passkey.h"

#include <openssl/sha.h>

#include <base/logging.h>
#include <base/stl_util.h>
#include <chromeos/secure_blob.h>
#include <chromeos/utility.h>

#include "cryptohome/crypto.h"

namespace cryptohome {
using chromeos::SecureBlob;
using std::string;

UsernamePasskey::UsernamePasskey(const char *username,
                                 const chromeos::Blob& passkey)
    : username_(username, strlen(username)),
      passkey_() {
  passkey_.assign(passkey.begin(), passkey.end());
}

UsernamePasskey::~UsernamePasskey() {
}

void UsernamePasskey::Assign(const Credentials& rhs) {
  username_.assign(rhs.username());
  key_data_ = rhs.key_data();
  SecureBlob passkey;
  rhs.GetPasskey(&passkey);
  passkey_.assign(passkey.begin(), passkey.end());
}

void UsernamePasskey::set_key_data(const KeyData& data) {
  key_data_ = data;
}

const KeyData& UsernamePasskey::key_data() const {
  return key_data_;
}

std::string UsernamePasskey::username() const {
  return username_;
}

string UsernamePasskey::GetObfuscatedUsername(
    const chromeos::Blob &system_salt) const {
  CHECK(!username_.empty());

  SHA_CTX ctx;
  unsigned char md_value[SHA_DIGEST_LENGTH];

  SHA1_Init(&ctx);
  SHA1_Update(&ctx, vector_as_array(&system_salt), system_salt.size());
  SHA1_Update(&ctx, username_.c_str(), username_.length());
  SHA1_Final(md_value, &ctx);

  chromeos::Blob md_blob(md_value,
               md_value + (SHA_DIGEST_LENGTH * sizeof(unsigned char)));

  return chromeos::AsciiEncode(md_blob);
}

void UsernamePasskey::GetPasskey(SecureBlob* passkey) const {
  *passkey = passkey_;
}

}  // namespace cryptohome
