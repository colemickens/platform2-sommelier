// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBHWSEC_CRYPTO_UTILITY_H_
#define LIBHWSEC_CRYPTO_UTILITY_H_

#include <cstdint>
#include <string>
#include <vector>

#include <base/optional.h>
#include <crypto/scoped_openssl_types.h>

#include "libhwsec/hwsec_export.h"

namespace hwsec {

// Convert RSA key (with public and/or private key set) key to the binary DER
// encoded SubjectPublicKeyInfo format.
//
// Return nullopt if key is null, or OpenSSL returned error.
HWSEC_EXPORT base::Optional<std::vector<uint8_t>>
RsaKeyToSubjectPublicKeyInfoBytes(const crypto::ScopedRSA& key);

// Convert ECC key (with public and/or private key set) key to the binary DER
// encoded SubjectPublicKeyInfo format.
//
// Return nullopt if key is null, or OpenSSL returned error.
HWSEC_EXPORT base::Optional<std::vector<uint8_t>>
EccKeyToSubjectPublicKeyInfoBytes(const crypto::ScopedEC_KEY& key);

}  // namespace hwsec

#endif  // LIBHWSEC_CRYPTO_UTILITY_H_
