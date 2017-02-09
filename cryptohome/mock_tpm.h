// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_MOCK_TPM_H_
#define CRYPTOHOME_MOCK_TPM_H_

#include "cryptohome/tpm.h"

#include <set>

#include <base/logging.h>
#include <brillo/secure_blob.h>
#include <gmock/gmock.h>

namespace cryptohome {
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

class Crypto;

class MockTpm : public Tpm {
 public:
  MockTpm();
  ~MockTpm();
  MOCK_METHOD0(GetVersion, TpmVersion());
  MOCK_METHOD4(EncryptBlob, TpmRetryAction(TpmKeyHandle,
                                           const brillo::SecureBlob&,
                                           const brillo::SecureBlob&,
                                           brillo::SecureBlob*));
  MOCK_METHOD4(DecryptBlob, TpmRetryAction(TpmKeyHandle,
                                           const brillo::SecureBlob&,
                                           const brillo::SecureBlob&,
                                           brillo::SecureBlob*));
  MOCK_METHOD2(GetPublicKeyHash, TpmRetryAction(TpmKeyHandle,
                                                brillo::SecureBlob*));
  MOCK_METHOD1(GetOwnerPassword, bool(brillo::Blob*));
  MOCK_METHOD0(IsEnabled, bool());
  MOCK_METHOD1(SetIsEnabled, void(bool));
  MOCK_METHOD0(IsOwned, bool());
  MOCK_METHOD1(SetIsOwned, void(bool));
  MOCK_METHOD2(PerformEnabledOwnedCheck, bool(bool*, bool*));
  MOCK_METHOD0(IsInitialized, bool());
  MOCK_METHOD1(SetIsInitialized, void(bool));
  MOCK_METHOD0(IsBeingOwned, bool());
  MOCK_METHOD1(SetIsBeingOwned, void(bool));
  MOCK_METHOD2(GetRandomData, bool(size_t, brillo::Blob*));
  MOCK_METHOD3(DefineNvram, bool(uint32_t, size_t, uint32_t));
  MOCK_METHOD2(WriteNvram, bool(uint32_t, const brillo::SecureBlob&));
  MOCK_METHOD2(ReadNvram, bool(uint32_t, brillo::SecureBlob*));
  MOCK_METHOD1(DestroyNvram, bool(uint32_t));
  MOCK_METHOD1(IsNvramDefined, bool(uint32_t));
  MOCK_METHOD1(IsNvramLocked, bool(uint32_t));
  MOCK_METHOD1(WriteLockNvram, bool(uint32_t));
  MOCK_METHOD1(GetNvramSize, unsigned int(uint32_t));
  MOCK_METHOD1(GetEndorsementPublicKey, bool(brillo::SecureBlob*));
  MOCK_METHOD1(GetEndorsementCredential, bool(brillo::SecureBlob*));
  MOCK_METHOD9(MakeIdentity, bool(brillo::SecureBlob*,
                                  brillo::SecureBlob*,
                                  brillo::SecureBlob*,
                                  brillo::SecureBlob*,
                                  brillo::SecureBlob*,
                                  brillo::SecureBlob*,
                                  brillo::SecureBlob*,
                                  brillo::SecureBlob*,
                                  brillo::SecureBlob*));
  MOCK_METHOD6(QuotePCR, bool(int,
                              const brillo::SecureBlob&,
                              const brillo::SecureBlob&,
                              brillo::SecureBlob*,
                              brillo::SecureBlob*,
                              brillo::SecureBlob*));
  MOCK_METHOD2(SealToPCR0, bool(const brillo::Blob&, brillo::Blob*));
  MOCK_METHOD2(Unseal, bool(const brillo::Blob&, brillo::Blob*));
  MOCK_METHOD7(CreateCertifiedKey, bool(const brillo::SecureBlob&,
                                        const brillo::SecureBlob&,
                                        brillo::SecureBlob*,
                                        brillo::SecureBlob*,
                                        brillo::SecureBlob*,
                                        brillo::SecureBlob*,
                                        brillo::SecureBlob*));
  MOCK_METHOD3(CreateDelegate, bool(const brillo::SecureBlob&,
                                    brillo::SecureBlob*,
                                    brillo::SecureBlob*));
  MOCK_METHOD6(ActivateIdentity, bool(const brillo::SecureBlob&,
                                      const brillo::SecureBlob&,
                                      const brillo::SecureBlob&,
                                      const brillo::SecureBlob&,
                                      const brillo::SecureBlob&,
                                      brillo::SecureBlob*));
  MOCK_METHOD4(Sign, bool(const brillo::SecureBlob&,
                          const brillo::SecureBlob&,
                          int,
                          brillo::SecureBlob*));
  MOCK_METHOD5(CreatePCRBoundKey, bool(int,
                                       const brillo::SecureBlob&,
                                       brillo::SecureBlob*,
                                       brillo::SecureBlob*,
                                       brillo::SecureBlob*));
  MOCK_METHOD4(VerifyPCRBoundKey, bool(int,
                                       const brillo::SecureBlob&,
                                       const brillo::SecureBlob&,
                                       const brillo::SecureBlob&));
  MOCK_METHOD2(ExtendPCR, bool(int, const brillo::SecureBlob&));
  MOCK_METHOD2(ReadPCR, bool(int, brillo::SecureBlob*));
  MOCK_METHOD0(IsEndorsementKeyAvailable, bool());
  MOCK_METHOD0(CreateEndorsementKey, bool());
  MOCK_METHOD2(TakeOwnership, bool(int, const brillo::SecureBlob&));
  MOCK_METHOD1(InitializeSrk, bool(const brillo::SecureBlob&));
  MOCK_METHOD2(ChangeOwnerPassword, bool(const brillo::SecureBlob&,
                                         const brillo::SecureBlob&));
  MOCK_METHOD1(TestTpmAuth, bool(const brillo::SecureBlob&));
  MOCK_METHOD1(SetOwnerPassword, void(const brillo::SecureBlob&));
  MOCK_METHOD1(IsTransient, bool(TpmRetryAction));
  MOCK_METHOD3(WrapRsaKey, bool(const brillo::SecureBlob&,
                                const brillo::SecureBlob&,
                                brillo::SecureBlob*));
  MOCK_METHOD2(LoadWrappedKey, TpmRetryAction(const brillo::SecureBlob&,
                                              ScopedKeyHandle*));
  MOCK_METHOD2(LegacyLoadCryptohomeKey, bool(ScopedKeyHandle*,
                                             brillo::SecureBlob*));
  MOCK_METHOD1(CloseHandle, void(TpmKeyHandle));
  MOCK_METHOD2(GetStatus, void(TpmKeyHandle, TpmStatusInfo*));
  MOCK_METHOD4(GetDictionaryAttackInfo, bool(int*, int*, bool*, int*));
  MOCK_METHOD2(ResetDictionaryAttackMitigation,
               bool(const brillo::SecureBlob&, const brillo::SecureBlob&));
  MOCK_METHOD0(DeclareTpmFirmwareStable, void());
  MOCK_METHOD1(RemoveOwnerDependency, bool(TpmOwnerDependency));
  MOCK_METHOD0(ClearStoredPassword, bool());

 private:
  TpmRetryAction Xor(TpmKeyHandle _key,
                     const brillo::SecureBlob& plaintext,
                     const brillo::SecureBlob& key,
                     brillo::SecureBlob* ciphertext) {
    brillo::SecureBlob local_data_out(plaintext.size());
    for (unsigned int i = 0; i < local_data_out.size(); i++) {
      local_data_out[i] = plaintext[i] ^ 0x1e;
    }
    ciphertext->swap(local_data_out);
    return kTpmRetryNone;
  }

  bool FakeGetRandomData(size_t num_bytes, brillo::Blob* blob) {
    blob->resize(num_bytes);
    return true;
  }

  bool FakeExtendPCR(int index, const brillo::SecureBlob& value) {
    extended_pcrs_.insert(index);
    return true;
  }

  bool FakeReadPCR(int index, brillo::SecureBlob* value) {
    value->assign(20, extended_pcrs_.count(index) ? 0xAA : 0);
    return true;
  }

  std::set<int> extended_pcrs_;
};
}  // namespace cryptohome

#endif  // CRYPTOHOME_MOCK_TPM_H_
