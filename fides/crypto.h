// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains declarations for crypto operations used by fides. The
// purpose of this file is to isolate implementations which will rely on
// specific crypto libraries from the consuming code that makes use of the
// operations. This simplifies porting to new crypto libraries and enables
// compile-time selection of the crypto library to use.

#ifndef FIDES_CRYPTO_H_
#define FIDES_CRYPTO_H_

#include <vector>

#include "fides/blob_ref.h"

namespace fides {
namespace crypto {

// Digest algorithms.
enum DigestAlgorithm {
  kDigestSha256,
};

// Computes the message digest of |data| and places it in |digest|. Returns true
// on success.
bool ComputeDigest(DigestAlgorithm algorithm,
                   BlobRef data,
                   std::vector<uint8_t>* digest);

// Verify a digest. Computes the digest over |data| and verifies that it matches
// the expected |digest|. Returns true if and only if there is a match.
bool VerifyDigest(DigestAlgorithm algorithm, BlobRef data, BlobRef digest);

}  // namespace crypto
}  // namespace fides

#endif  // FIDES_CRYPTO_H_
