// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "u2fd/util.h"

#include <string>
#include <vector>

#include <base/logging.h>
#include <crypto/scoped_openssl_types.h>
#include <openssl/bn.h>
#include <openssl/ecdsa.h>
#include <openssl/sha.h>

namespace u2f {
namespace util {

template <>
void AppendToVector(const std::vector<uint8_t>& from,
                    std::vector<uint8_t>* to) {
  to->insert(to->end(), from.begin(), from.end());
}

template <>
void AppendToVector(const std::string& from, std::vector<uint8_t>* to) {
  to->insert(to->end(), from.begin(), from.end());
}

void AppendSubstringToVector(const std::string& from,
                             int start,
                             int length,
                             std::vector<uint8_t>* to) {
  to->insert(to->end(), from.begin() + start, from.begin() + start + length);
}

base::Optional<std::vector<uint8_t>> SignatureToDerBytes(const uint8_t* r,
                                                         const uint8_t* s) {
  crypto::ScopedECDSA_SIG sig(ECDSA_SIG_new());

  if (!BN_bin2bn(r, 32, sig->r) || !BN_bin2bn(s, 32, sig->s)) {
    LOG(ERROR) << "Failed to initialize ECDSA_SIG";
    return base::nullopt;
  }

  int sig_len = i2d_ECDSA_SIG(sig.get(), nullptr);

  std::vector<uint8_t> signature(sig_len);
  uint8_t* sig_ptr = &signature[0];

  if (i2d_ECDSA_SIG(sig.get(), &sig_ptr) != sig_len) {
    LOG(ERROR) << "DER encoding returned unexpected length";
    return base::nullopt;
  }

  return signature;
}

std::vector<uint8_t> Sha256(const std::vector<uint8_t>& data) {
  std::vector<uint8_t> hash(SHA256_DIGEST_LENGTH);
  SHA256_CTX sha_context;

  SHA256_Init(&sha_context);
  SHA256_Update(&sha_context, &data.front(), data.size());
  SHA256_Final(&hash.front(), &sha_context);

  return hash;
}

}  // namespace util
}  // namespace u2f
