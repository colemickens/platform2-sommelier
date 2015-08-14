// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/privet/openssl_utils.h"

#include <algorithm>

#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <base/logging.h>

namespace weave {
namespace privet {

std::vector<uint8_t> HmacSha256(const std::vector<uint8_t>& key,
                                const std::vector<uint8_t>& data) {
  std::vector<uint8_t> mac(kSha256OutputSize);
  uint32_t len = 0;
  CHECK(HMAC(EVP_sha256(), key.data(), key.size(), data.data(), data.size(),
             mac.data(), &len));
  CHECK_EQ(len, kSha256OutputSize);
  return mac;
}

}  // namespace privet
}  // namespace weave
