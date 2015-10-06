// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fides/crypto.h"

#include <stdint.h>

#include <memory>

#include <base/logging.h>

#include <openssl/crypto.h>
#include <openssl/evp.h>

namespace fides {
namespace crypto {

bool ComputeDigest(DigestAlgorithm algorithm,
                   BlobRef data,
                   std::vector<uint8_t>* digest) {
  const EVP_MD* md = nullptr;
  switch (algorithm) {
    case kDigestSha256:
      md = EVP_sha256();
      break;
  }

  if (!md) {
    LOG(ERROR) << "Failed to determine algorithm " << algorithm;
    return false;
  }

  EVP_MD_CTX* md_ctx = EVP_MD_CTX_create();
  if (!md_ctx) {
    LOG(ERROR) << "Failed to create message digest context.";
    return false;
  }

  const unsigned int digest_size = EVP_MD_size(md);
  digest->resize(digest_size);
  unsigned int actual_digest_size = digest_size;
  if (!EVP_DigestInit_ex(md_ctx, md, nullptr) ||
      !EVP_DigestUpdate(md_ctx, data.data(), data.size()) ||
      !EVP_DigestFinal_ex(md_ctx, digest->data(), &actual_digest_size)) {
    LOG(ERROR) << "Failed to compute digest.";
    EVP_MD_CTX_destroy(md_ctx);
    return false;
  }

  EVP_MD_CTX_destroy(md_ctx);
  return digest_size == actual_digest_size;
}

bool VerifyDigest(DigestAlgorithm algorithm, BlobRef data, BlobRef digest) {
  std::vector<uint8_t> actual_digest;
  if (!ComputeDigest(algorithm, data, &actual_digest))
    return false;

  // Do a constant time memory comparison here. It's not strictly needed, but
  // one never knows who'll end up calling this code.
  return digest.size() == actual_digest.size() &&
         CRYPTO_memcmp(digest.data(), actual_digest.data(), digest.size()) == 0;
}

}  // namespace crypto
}  // namespace fides
