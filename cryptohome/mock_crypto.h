// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_MOCK_CRYPTO_H_
#define CRYPTOHOME_MOCK_CRYPTO_H_

#include "cryptohome/crypto.h"

#include <string>

#include <chromeos/secure_blob.h>
#include <gmock/gmock.h>

#include "attestation.pb.h"  // NOLINT(build/include)

namespace cryptohome {

class Platform;

class MockCrypto : public Crypto {
 public:
  MockCrypto(): Crypto(NULL) { set_use_tpm(true); }
  virtual ~MockCrypto() {}

  MOCK_CONST_METHOD2(EncryptWithTpm, bool(const chromeos::SecureBlob&,
                                          std::string*));
  MOCK_CONST_METHOD2(DecryptWithTpm, bool(const std::string&,
                                          chromeos::SecureBlob*));
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_MOCK_CRYPTO_H_
