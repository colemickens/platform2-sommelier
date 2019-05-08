// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include <base/optional.h>
#include <crypto/scoped_openssl_types.h>
#include <openssl/x509.h>

#include "libhwsec/crypto_utility.h"

namespace {

// The wrapper of OpenSSL i2d series function. It takes a OpenSSL i2d function
// and apply to |object|.
//
// The wrapper will always accept the non-const pointer of the object since
// unique_ptr::get will only return the non-const version. It will break the
// type deduction of template.
template <typename OpenSSLType>
base::Optional<std::vector<uint8_t>> OpenSSLObjectToBytes(
    int (*i2d_convert_function)(OpenSSLType*, unsigned char**),
    typename std::remove_const<OpenSSLType>::type* object) {
  if (object == nullptr) {
    return base::nullopt;
  }

  unsigned char* openssl_buffer = nullptr;

  int size = i2d_convert_function(object, &openssl_buffer);
  if (size < 0) {
    return base::nullopt;
  }

  crypto::ScopedOpenSSLBytes scoped_buffer(openssl_buffer);
  return std::vector<uint8_t>(openssl_buffer, openssl_buffer + size);
}

}  // namespace

namespace hwsec {

base::Optional<std::vector<uint8_t>> RsaKeyToSubjectPublicKeyInfoBytes(
    const crypto::ScopedRSA& key) {
  return OpenSSLObjectToBytes(i2d_RSA_PUBKEY, key.get());
}

base::Optional<std::vector<uint8_t>> EccKeyToSubjectPublicKeyInfoBytes(
    const crypto::ScopedEC_KEY& key) {
  return OpenSSLObjectToBytes(i2d_EC_PUBKEY, key.get());
}

}  // namespace hwsec
