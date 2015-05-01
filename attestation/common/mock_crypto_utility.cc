// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "attestation/common/mock_crypto_utility.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::WithArgs;

namespace {

bool FakeRandom(size_t num_bytes, std::string* output) {
  *output = std::string(num_bytes, 'A');
  return true;
}

bool CopyString(const std::string& s1, std::string* s2) {
  *s2 = s1;
  return true;
}

}  // namespace

namespace attestation {

MockCryptoUtility::MockCryptoUtility() {
  ON_CALL(*this, GetRandom(_, _)).WillByDefault(Invoke(FakeRandom));
  ON_CALL(*this, CreateSealedKey(_, _)).WillByDefault(Return(true));
  ON_CALL(*this, UnsealKey(_, _, _)).WillByDefault(Return(true));
  ON_CALL(*this, EncryptData(_, _, _, _))
      .WillByDefault(WithArgs<0, 3>(Invoke(CopyString)));
  ON_CALL(*this, DecryptData(_, _, _))
      .WillByDefault(WithArgs<0, 2>(Invoke(CopyString)));
  ON_CALL(*this, GetRSASubjectPublicKeyInfo(_, _))
      .WillByDefault(Invoke(CopyString));
}

MockCryptoUtility::~MockCryptoUtility() {}

}  // namespace attestation
