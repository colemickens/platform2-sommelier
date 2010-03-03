// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/username_passhash.h"

#include <openssl/sha.h>

#include "base/logging.h"
#include "chromeos/utility.h"

namespace cryptohome {
using namespace chromeos;
using std::string;

UsernamePasshash::UsernamePasshash(const char *username,
                                   const int username_length,
                                   const char *passhash,
                                   const int passhash_length)
    : username_(username, username_length),
      passhash_(passhash, passhash_length) {
}

void UsernamePasshash::GetFullUsername(char *name_buffer, int length) const {
  strncpy(name_buffer, username_.c_str(), length);
}

void UsernamePasshash::GetPartialUsername(char *name_buffer, int length) const {
  size_t at_index = username_.find('@');
  string partial = username_.substr(0, at_index);
  strncpy(name_buffer, partial.c_str(), length);
}

string UsernamePasshash::GetObfuscatedUsername(const Blob &system_salt) const {
  CHECK(!username_.empty());

  SHA_CTX ctx;
  unsigned char md_value[SHA_DIGEST_LENGTH];

  SHA1_Init(&ctx);
  SHA1_Update(&ctx, &system_salt[0], system_salt.size());
  SHA1_Update(&ctx, username_.c_str(), username_.length());
  SHA1_Final(md_value, &ctx);

  Blob md_blob(md_value,
               md_value + (SHA_DIGEST_LENGTH * sizeof(unsigned char)));

  return AsciiEncode(md_blob);
}

string UsernamePasshash::GetPasswordWeakHash(const Blob &system_salt) const {
  return passhash_;
}

}  // namespace cryptohome
