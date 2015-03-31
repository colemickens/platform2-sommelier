// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_MOCK_TPM_H_
#define CRYPTOHOME_MOCK_TPM_H_

#include "cryptohome/tpm.h"

#include <set>

#include <base/logging.h>
#include <chromeos/secure_blob.h>
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
  MOCK_METHOD5(EncryptBlob, TpmRetryAction(TSS_HCONTEXT, TSS_HKEY,
                                           const chromeos::SecureBlob&,
                                           const chromeos::SecureBlob&,
                                           chromeos::SecureBlob*));
  MOCK_METHOD5(DecryptBlob, TpmRetryAction(TSS_HCONTEXT, TSS_HKEY,
                                           const chromeos::SecureBlob&,
                                           const chromeos::SecureBlob&,
                                           chromeos::SecureBlob*));
  MOCK_METHOD3(GetPublicKeyHash, TpmRetryAction(TSS_HCONTEXT,
                                                TSS_HKEY,
                                                chromeos::SecureBlob*));
  MOCK_METHOD1(GetOwnerPassword, bool(chromeos::Blob*));
  MOCK_CONST_METHOD0(IsEnabled, bool());
  MOCK_METHOD1(SetIsEnabled, void(bool));
  MOCK_CONST_METHOD0(IsOwned, bool());
  MOCK_METHOD1(SetIsOwned, void(bool));
  MOCK_METHOD2(PerformEnabledOwnedCheck, bool(bool*, bool*));
  MOCK_CONST_METHOD0(IsInitialized, bool());
  MOCK_METHOD1(SetIsInitialized, void(bool));
  MOCK_CONST_METHOD0(IsBeingOwned, bool());
  MOCK_METHOD1(SetIsBeingOwned, void(bool));
  MOCK_METHOD2(GetRandomData, bool(size_t, chromeos::Blob*));
  MOCK_METHOD2(DefineLockOnceNvram, bool(uint32_t, size_t));
  MOCK_METHOD2(WriteNvram, bool(uint32_t, const chromeos::SecureBlob&));
  MOCK_METHOD2(ReadNvram, bool(uint32_t, chromeos::SecureBlob*));
  MOCK_METHOD1(DestroyNvram, bool(uint32_t));
  MOCK_METHOD1(IsNvramDefined, bool(uint32_t));
  MOCK_METHOD1(IsNvramLocked, bool(uint32_t));
  MOCK_METHOD1(GetNvramSize, unsigned int(uint32_t));
  MOCK_METHOD1(GetEndorsementPublicKey, bool(chromeos::SecureBlob*));
  MOCK_METHOD1(GetEndorsementCredential, bool(chromeos::SecureBlob*));
  MOCK_METHOD9(MakeIdentity, bool(chromeos::SecureBlob*,
                                  chromeos::SecureBlob*,
                                  chromeos::SecureBlob*,
                                  chromeos::SecureBlob*,
                                  chromeos::SecureBlob*,
                                  chromeos::SecureBlob*,
                                  chromeos::SecureBlob*,
                                  chromeos::SecureBlob*,
                                  chromeos::SecureBlob*));
  MOCK_METHOD6(QuotePCR, bool(int,
                              const chromeos::SecureBlob&,
                              const chromeos::SecureBlob&,
                              chromeos::SecureBlob*,
                              chromeos::SecureBlob*,
                              chromeos::SecureBlob*));
  MOCK_METHOD2(SealToPCR0, bool(const chromeos::Blob&, chromeos::Blob*));
  MOCK_METHOD2(Unseal, bool(const chromeos::Blob&, chromeos::Blob*));
  MOCK_METHOD7(CreateCertifiedKey, bool(const chromeos::SecureBlob&,
                                        const chromeos::SecureBlob&,
                                        chromeos::SecureBlob*,
                                        chromeos::SecureBlob*,
                                        chromeos::SecureBlob*,
                                        chromeos::SecureBlob*,
                                        chromeos::SecureBlob*));
  MOCK_METHOD3(CreateDelegate, bool(const chromeos::SecureBlob&,
                                    chromeos::SecureBlob*,
                                    chromeos::SecureBlob*));
  MOCK_METHOD6(ActivateIdentity, bool(const chromeos::SecureBlob&,
                                      const chromeos::SecureBlob&,
                                      const chromeos::SecureBlob&,
                                      const chromeos::SecureBlob&,
                                      const chromeos::SecureBlob&,
                                      chromeos::SecureBlob*));
  MOCK_METHOD3(Sign, bool(const chromeos::SecureBlob&,
                          const chromeos::SecureBlob&,
                          chromeos::SecureBlob*));
  MOCK_METHOD4(CreatePCRBoundKey, bool(int,
                                       const chromeos::SecureBlob&,
                                       chromeos::SecureBlob*,
                                       chromeos::SecureBlob*));
  MOCK_METHOD3(VerifyPCRBoundKey, bool(int,
                                       const chromeos::SecureBlob&,
                                       const chromeos::SecureBlob&));
  MOCK_METHOD2(ExtendPCR, bool(int, const chromeos::SecureBlob&));
  MOCK_METHOD2(ReadPCR, bool(int, chromeos::SecureBlob*));
  MOCK_METHOD0(ConnectContext, TSS_HCONTEXT());
  MOCK_CONST_METHOD1(CloseContext, void(TSS_HCONTEXT));
  MOCK_METHOD1(IsEndorsementKeyAvailable, bool(TSS_HCONTEXT));
  MOCK_METHOD1(CreateEndorsementKey, bool(TSS_HCONTEXT));
  MOCK_METHOD3(TakeOwnership, bool(TSS_HCONTEXT, int,
                                   const chromeos::SecureBlob&));
  MOCK_METHOD2(InitializeSrk, bool(TSS_HCONTEXT, const chromeos::SecureBlob&));
  MOCK_METHOD3(ChangeOwnerPassword, bool(TSS_HCONTEXT,
                                         const chromeos::SecureBlob&,
                                         const chromeos::SecureBlob&));
  MOCK_METHOD2(TestTpmAuth, bool(TSS_HCONTEXT, const chromeos::SecureBlob&));
  MOCK_METHOD1(SetOwnerPassword, void(const chromeos::SecureBlob&));
  MOCK_METHOD1(IsTransient, bool(TpmRetryAction));
  MOCK_METHOD2(CreateWrappedRsaKey, bool(TSS_HCONTEXT,
                                         chromeos::SecureBlob*));
  MOCK_CONST_METHOD3(LoadWrappedKey, TpmRetryAction(TSS_HCONTEXT,
                                                    const chromeos::SecureBlob&,
                                                    TSS_HKEY*));
  MOCK_CONST_METHOD4(LoadKeyByUuid, bool(TSS_HCONTEXT, TSS_UUID, TSS_HKEY*,
                                         chromeos::SecureBlob*));
  MOCK_METHOD3(GetStatus, void(TSS_HCONTEXT, TSS_HKEY, TpmStatusInfo*));
  MOCK_METHOD4(GetDictionaryAttackInfo, bool(int*, int*, bool*, int*));
  MOCK_METHOD2(ResetDictionaryAttackMitigation,
               bool(const chromeos::SecureBlob&, const chromeos::SecureBlob&));

 private:
  TpmRetryAction Xor(TSS_HCONTEXT _context,
                     TSS_HKEY _key,
                     const chromeos::SecureBlob& plaintext,
                     const chromeos::SecureBlob& key,
                     chromeos::SecureBlob* ciphertext) {
    chromeos::SecureBlob local_data_out(plaintext.size());
    for (unsigned int i = 0; i < local_data_out.size(); i++) {
      local_data_out[i] = plaintext[i] ^ 0x1e;
    }
    ciphertext->swap(local_data_out);
    return kTpmRetryNone;
  }

  bool FakeGetRandomData(size_t num_bytes, chromeos::Blob* blob) {
    blob->resize(num_bytes);
    return true;
  }

  bool FakeExtendPCR(int index, const chromeos::SecureBlob& value) {
    extended_pcrs_.insert(index);
    return true;
  }

  bool FakeReadPCR(int index, chromeos::SecureBlob* value) {
    value->assign(20, extended_pcrs_.count(index) ? 0xAA : 0);
    return true;
  }

  std::set<int> extended_pcrs_;
};
}  // namespace cryptohome

#endif  // CRYPTOHOME_MOCK_TPM_H_
