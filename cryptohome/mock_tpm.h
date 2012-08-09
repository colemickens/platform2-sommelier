// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_MOCK_TPM_H_
#define CRYPTOHOME_MOCK_TPM_H_

#include "tpm.h"

#include <base/logging.h>
#include <chromeos/secure_blob.h>
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
    ON_CALL(*this, Connect(_))
        .WillByDefault(Return(true));
    ON_CALL(*this, GetPublicKey(_, _))
        .WillByDefault(Invoke(this, &MockTpm::GetBlankPublicKey));
    ON_CALL(*this, Init(_, _))
        .WillByDefault(Return(true));
    ON_CALL(*this, GetEndorsementPublicKey(_))
        .WillByDefault(Return(true));
    ON_CALL(*this, MakeIdentity(_, _, _, _, _, _, _, _))
        .WillByDefault(Return(true));
    ON_CALL(*this, QuotePCR0(_, _, _, _, _))
        .WillByDefault(Return(true));
    ON_CALL(*this, SealToPCR0(_, _))
        .WillByDefault(Return(true));
    ON_CALL(*this, Unseal(_, _))
        .WillByDefault(Return(true));
    ON_CALL(*this, GetRandomData(_, _))
        .WillByDefault(Invoke(this, &MockTpm::FakeGetRandomData));
  }
  ~MockTpm() {}
  MOCK_METHOD2(Init, bool(Platform*, bool));
  MOCK_METHOD0(IsConnected, bool());
  MOCK_CONST_METHOD0(IsOwned, bool());
  MOCK_METHOD1(Connect, bool(TpmRetryAction*));  // NOLINT
  MOCK_METHOD0(Disconnect, void());
  MOCK_METHOD4(Encrypt, bool(const chromeos::SecureBlob&,
                             const chromeos::SecureBlob&,
                                   chromeos::SecureBlob*, TpmRetryAction*));
  MOCK_METHOD4(Decrypt, bool(const chromeos::SecureBlob&,
                             const chromeos::SecureBlob&,
                                   chromeos::SecureBlob*, TpmRetryAction*));
  MOCK_METHOD2(GetPublicKey, bool(chromeos::SecureBlob*, TpmRetryAction*));
  MOCK_METHOD1(GetOwnerPassword, bool(chromeos::Blob*));
  MOCK_METHOD1(RemoveOwnerDependency, void(Tpm::TpmOwnerDependency));
  MOCK_CONST_METHOD0(IsEnabled, bool());
  MOCK_METHOD2(GetRandomData, bool(size_t, chromeos::Blob*));
  MOCK_METHOD2(DefineLockOnceNvram, bool(uint32_t, size_t));
  MOCK_METHOD2(WriteNvram, bool(uint32_t, const chromeos::SecureBlob&));
  MOCK_METHOD2(ReadNvram, bool(uint32_t, chromeos::SecureBlob*));
  MOCK_METHOD1(DestroyNvram, bool(uint32_t));
  MOCK_METHOD1(IsNvramDefined, bool(uint32_t));
  MOCK_METHOD1(IsNvramLocked, bool(uint32_t));
  MOCK_METHOD1(GetNvramSize, unsigned int(uint32_t));
  MOCK_METHOD1(GetEndorsementPublicKey, bool(chromeos::SecureBlob*));
  MOCK_METHOD8(MakeIdentity, bool(chromeos::SecureBlob*,
                                  chromeos::SecureBlob*,
                                  chromeos::SecureBlob*,
                                  chromeos::SecureBlob*,
                                  chromeos::SecureBlob*,
                                  chromeos::SecureBlob*,
                                  chromeos::SecureBlob*,
                                  chromeos::SecureBlob*));
  MOCK_METHOD5(QuotePCR0, bool(const chromeos::SecureBlob&,
                               const chromeos::SecureBlob&,
                               chromeos::SecureBlob*,
                               chromeos::SecureBlob*,
                               chromeos::SecureBlob*));
  MOCK_METHOD2(SealToPCR0, bool(const chromeos::Blob&, chromeos::Blob*));
  MOCK_METHOD2(Unseal, bool(const chromeos::Blob&, chromeos::Blob*));

 private:
  bool Xor(const chromeos::SecureBlob& plaintext,
           const chromeos::SecureBlob& key,
           chromeos::SecureBlob* ciphertext,
           TpmRetryAction* retry_action) {
    chromeos::SecureBlob local_data_out(plaintext.size());
    for (unsigned int i = 0; i < local_data_out.size(); i++) {
      local_data_out[i] = plaintext[i] ^ 0x1e;
    }
    ciphertext->swap(local_data_out);
    return true;
  }

  bool GetBlankPublicKey(chromeos::SecureBlob* blob,
                         TpmRetryAction* retry_action) {
    blob->resize(0);
    return true;
  }

  bool FakeGetRandomData(size_t num_bytes, chromeos::Blob* blob) {
    blob->resize(num_bytes);
    return true;
  }
};
}  // namespace cryptohome

#endif  // CRYPTOHOME_MOCK_TPM_H_
