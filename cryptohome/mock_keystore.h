// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_MOCK_KEYSTORE_H_
#define CRYPTOHOME_MOCK_KEYSTORE_H_

#include "keystore.h"

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

  MOCK_METHOD2(Read, bool(const std::string& name,
                          chromeos::SecureBlob* key_data));
  MOCK_METHOD2(Write, bool(const std::string& name,
                           const chromeos::SecureBlob& key_data));
  MOCK_METHOD1(Delete, bool(const std::string& name));
  MOCK_METHOD2(Register, bool(const chromeos::SecureBlob&,
                              const chromeos::SecureBlob&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockKeyStore);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_MOCK_KEYSTORE_H_
