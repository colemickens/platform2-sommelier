// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privetd/openssl_utils.h"

#include <algorithm>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <base/logging.h>

namespace privetd {

chromeos::Blob HmacSha256(const chromeos::SecureBlob& key,
                          const chromeos::Blob& data) {
  chromeos::Blob mac(kSha256OutputSize);
  uint32_t len = 0;
  CHECK(HMAC(EVP_sha256(), key.const_data(), key.size(), data.data(),
             data.size(), mac.data(), &len));
  CHECK_EQ(len, kSha256OutputSize);
  return mac;
}

std::string Base64Encode(const chromeos::Blob& input) {
  BIO* base64 = BIO_new(BIO_f_base64());
  BIO_set_flags(base64, BIO_FLAGS_BASE64_NO_NL);
  BIO* bio = BIO_new(BIO_s_mem());
  bio = BIO_push(base64, bio);
  BIO_write(bio, input.data(), input.size());
  static_cast<void>(BIO_flush(bio));
  char* data = nullptr;
  size_t length = BIO_get_mem_data(bio, &data);
  std::string output(data, length);
  chromeos::SecureMemset(data, 0, length);
  BIO_free_all(bio);
  return output;
}

chromeos::Blob Base64Decode(std::string input) {
  chromeos::Blob result(input.size(), 0);
  BIO* base64 = BIO_new(BIO_f_base64());
  BIO_set_flags(base64, BIO_FLAGS_BASE64_NO_NL);
  BIO* bio = BIO_new_mem_buf(&input.front(), input.length());
  bio = BIO_push(base64, bio);
  int size = BIO_read(bio, result.data(), input.size());
  BIO_free_all(bio);
  result.resize(std::max<int>(0, size));
  return result;
}

}  // namespace privetd
