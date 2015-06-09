// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_PRIVET_OPENSSL_UTILS_H_
#define BUFFET_PRIVET_OPENSSL_UTILS_H_

#include <string>
#include <vector>

#include <chromeos/secure_blob.h>

namespace privetd {

const size_t kSha256OutputSize = 32;

chromeos::Blob HmacSha256(const chromeos::SecureBlob& key,
                          const chromeos::Blob& data);

}  // namespace privetd

#endif  // BUFFET_PRIVET_OPENSSL_UTILS_H_
