// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/privet/openssl_utils.h"

#include <algorithm>

#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <base/logging.h>

namespace privetd {

chromeos::Blob HmacSha256(const chromeos::SecureBlob& key,
                          const chromeos::Blob& data) {
  chromeos::Blob mac(kSha256OutputSize);
  uint32_t len = 0;
  CHECK(HMAC(EVP_sha256(), key.data(), key.size(), data.data(), data.size(),
             mac.data(), &len));
  CHECK_EQ(len, kSha256OutputSize);
  return mac;
}

}  // namespace privetd
