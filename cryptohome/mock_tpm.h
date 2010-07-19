// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_MOCK_TPM_H_
#define CRYPTOHOME_MOCK_TPM_H_

#include "tpm.h"

#include <base/logging.h>
#include <chromeos/utility.h>

#include <gmock/gmock.h>

namespace cryptohome {
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

class Crypto;

class MockTpm : public Tpm {
 public:
  MockTpm() {
    ON_CALL(*this, Encrypt(_, _, _, _))
        .WillByDefault(Invoke(this, &MockTpm::Xor));
    ON_CALL(*this, Decrypt(_, _, _, _))
        .WillByDefault(Invoke(this, &MockTpm::Xor));
    ON_CALL(*this, IsConnected())
        .WillByDefault(Return(true));
    ON_CALL(*this, Connect())
        .WillByDefault(Return(true));
  }
  ~MockTpm() {}
  MOCK_METHOD2(Init, bool(Crypto*, bool));
  MOCK_CONST_METHOD0(IsConnected, bool());
  MOCK_METHOD0(Connect, bool());
  MOCK_METHOD0(Disconnect, void());
  MOCK_CONST_METHOD4(Encrypt, bool(const chromeos::Blob&, const chromeos::Blob&,
                                   const chromeos::Blob&, SecureBlob*));
  MOCK_CONST_METHOD4(Decrypt, bool(const chromeos::Blob&, const chromeos::Blob&,
                                   const chromeos::Blob&, SecureBlob*));
 private:
  bool Xor(const chromeos::Blob& data, const chromeos::Blob& password,
                const chromeos::Blob& salt, SecureBlob* data_out) {
    SecureBlob local_data_out(data.size());
    for (unsigned int i = 0; i < local_data_out.size(); i++) {
      local_data_out[i] = data[i] ^ 0x1e;
    }
    data_out->swap(local_data_out);
    return true;
  }
};
}  // namespace cryptohome

#endif  // CRYPTOHOME_MOCK_TPM_H_
