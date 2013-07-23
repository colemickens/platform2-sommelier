// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOCK_CRYPTO_H_
#define MOCK_CRYPTO_H_

#include "crypto.h"

#include <string>

#include "attestation.pb.h"
#include <chromeos/secure_blob.h>
#include <gmock/gmock.h>

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

#endif  /* !MOCK_CRYPTO_H_ */
