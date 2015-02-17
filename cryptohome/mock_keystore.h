// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_MOCK_KEYSTORE_H_
#define CRYPTOHOME_MOCK_KEYSTORE_H_

#include "cryptohome/keystore.h"

#include <string>

#include <chromeos/secure_blob.h>
#include <gmock/gmock.h>

using ::testing::_;
using ::testing::Return;

namespace cryptohome {

class MockKeyStore : public KeyStore {
 public:
  MockKeyStore();
  virtual ~MockKeyStore();

  MOCK_METHOD4(Read, bool(bool is_user_specific,
                          const std::string& username,
                          const std::string& name,
                          chromeos::SecureBlob* key_data));
  MOCK_METHOD4(Write, bool(bool is_user_specific,
                           const std::string& username,
                           const std::string& name,
                           const chromeos::SecureBlob& key_data));
  MOCK_METHOD3(Delete, bool(bool is_user_specific,
                            const std::string& username,
                            const std::string& name));
  MOCK_METHOD3(DeleteByPrefix, bool(bool is_user_specific,
                                    const std::string& username,
                                    const std::string& key_prefix));
  MOCK_METHOD6(Register, bool(bool is_user_specific,
                              const std::string& username,
                              const std::string& label,
                              const chromeos::SecureBlob& private_key_blob,
                              const chromeos::SecureBlob& public_key_der,
                              const chromeos::SecureBlob& certificate));
  MOCK_METHOD3(RegisterCertificate,
               bool(bool is_user_specific,
                    const std::string& username,
                    const chromeos::SecureBlob& certificate));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockKeyStore);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_MOCK_KEYSTORE_H_
