// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/obfuscated_username.h"

#include <cstdint>

#include <base/logging.h>
#include <openssl/sha.h>

#include "cryptohome/cryptolib.h"

namespace cryptohome {

std::string BuildObfuscatedUsername(const std::string& username,
                                    const brillo::SecureBlob& system_salt) {
  CHECK(!username.empty());
  SHA_CTX ctx;
  SHA1_Init(&ctx);
  SHA1_Update(&ctx, system_salt.data(), system_salt.size());
  SHA1_Update(&ctx, username.c_str(), username.length());
  brillo::Blob hash_value(SHA_DIGEST_LENGTH);
  SHA1_Final(hash_value.data(), &ctx);
  return CryptoLib::BlobToHex(hash_value);
}

}  // namespace cryptohome
