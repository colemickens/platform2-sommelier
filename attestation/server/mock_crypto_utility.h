// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATTESTATION_SERVER_MOCK_CRYPTO_UTILITY_H_
#define ATTESTATION_SERVER_MOCK_CRYPTO_UTILITY_H_

#include "attestation/server/crypto_utility.h"

#include <string>

#include <gmock/gmock.h>

namespace attestation {

class MockCryptoUtility : public CryptoUtility {
 public:
  MockCryptoUtility();
  ~MockCryptoUtility() override;

  MOCK_CONST_METHOD2(GetRandom, bool(size_t, std::string*));

  MOCK_METHOD2(CreateSealedKey, bool(std::string* aes_key,
                                     std::string* sealed_key));

  MOCK_METHOD4(EncryptData, bool(const std::string& data,
                                 const std::string& aes_key,
                                 const std::string& sealed_key,
                                 std::string* encrypted_data));

  MOCK_METHOD3(UnsealKey, bool(const std::string& encrypted_data,
                               std::string* aes_key,
                               std::string* sealed_key));

  MOCK_METHOD3(DecryptData, bool(const std::string& encrypted_data,
                                 const std::string& aes_key,
                                 std::string* data));
};

}  // namespace attestation

#endif  // ATTESTATION_SERVER_MOCK_CRYPTO_UTILITY_H_
