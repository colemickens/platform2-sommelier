// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_CHAPS_PROXY_MOCK_H_
#define CHAPS_CHAPS_PROXY_MOCK_H_

#include <string>
#include <vector>

#include <chromeos/secure_blob.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "chaps/chaps_interface.h"
#include "chaps/isolate.h"

namespace chaps {

// Defined in chaps.cc.
extern void EnableMockProxy(ChapsInterface* proxy,
                            chromeos::SecureBlob* isolate_credential,
                            bool is_initialized);
extern void DisableMockProxy();

// ChapsProxyMock is a mock of ChapsInterface.
class ChapsProxyMock : public ChapsInterface {
 public:
  explicit ChapsProxyMock(bool is_initialized)
    : isolate_credential_(
          IsolateCredentialManager::GetDefaultIsolateCredential()) {
    EnableMockProxy(this, &isolate_credential_, is_initialized);
  }

  virtual ~ChapsProxyMock() {
    DisableMockProxy();
  }

  MOCK_METHOD3(GetSlotList, uint32_t(const chromeos::SecureBlob&, bool,
                                     std::vector<uint64_t>*));
  MOCK_METHOD9(GetSlotInfo, uint32_t(const chromeos::SecureBlob&,
                                     uint64_t,
                                     std::vector<uint8_t>*,
                                     std::vector<uint8_t>*,
                                     uint64_t*,
                                     uint8_t*, uint8_t*,
                                     uint8_t*, uint8_t*));
  virtual uint32_t GetTokenInfo(const chromeos::SecureBlob&,
                                uint64_t slot_id,
                                std::vector<uint8_t>* label,
                                std::vector<uint8_t>* manufacturer_id,
                                std::vector<uint8_t>* model,
                                std::vector<uint8_t>* serial_number,
                                uint64_t* flags,
                                uint64_t* max_session_count,
                                uint64_t* session_count,
                                uint64_t* max_session_count_rw,
                                uint64_t* session_count_rw,
                                uint64_t* max_pin_len,
                                uint64_t* min_pin_len,
                                uint64_t* total_public_memory,
                                uint64_t* free_public_memory,
                                uint64_t* total_private_memory,
                                uint64_t* free_private_memory,
                                uint8_t* hardware_version_major,
                                uint8_t* hardware_version_minor,
                                uint8_t* firmware_version_major,
                                uint8_t* firmware_version_minor) {
    *flags = 1;
    return 0;
  }
  MOCK_METHOD3(GetMechanismList, uint32_t(const chromeos::SecureBlob&,
                                          uint64_t, std::vector<uint64_t>*));
  MOCK_METHOD6(GetMechanismInfo, uint32_t(const chromeos::SecureBlob&,
                                          uint64_t, uint64_t, uint64_t*,
                                          uint64_t*, uint64_t*));
  MOCK_METHOD4(InitToken, uint32_t(const chromeos::SecureBlob&,
                                   uint64_t,
                                   const std::string*,
                                   const std::vector<uint8_t>&));
  MOCK_METHOD3(InitPIN, uint32_t(const chromeos::SecureBlob&, uint64_t,
                                 const std::string*));
  MOCK_METHOD4(SetPIN, uint32_t(const chromeos::SecureBlob&, uint64_t,
                                const std::string*,
                                const std::string*));
  MOCK_METHOD4(OpenSession, uint32_t(const chromeos::SecureBlob&, uint64_t,
                                     uint64_t, uint64_t*));
  MOCK_METHOD2(CloseSession, uint32_t(const chromeos::SecureBlob&, uint64_t));
  MOCK_METHOD2(CloseAllSessions, uint32_t(const chromeos::SecureBlob&,
                                          uint64_t));
  MOCK_METHOD6(GetSessionInfo, uint32_t(const chromeos::SecureBlob&, uint64_t,
                                        uint64_t*, uint64_t*, uint64_t*,
                                        uint64_t*));
  MOCK_METHOD3(GetOperationState, uint32_t(const chromeos::SecureBlob&,
                                           uint64_t, std::vector<uint8_t>*));
  MOCK_METHOD5(SetOperationState, uint32_t(const chromeos::SecureBlob&,
                                           uint64_t,
                                           const std::vector<uint8_t>&,
                                           uint64_t,
                                           uint64_t));
  MOCK_METHOD4(Login, uint32_t(const chromeos::SecureBlob&, uint64_t, uint64_t,
                               const std::string*));
  MOCK_METHOD2(Logout, uint32_t(const chromeos::SecureBlob&, uint64_t));
  MOCK_METHOD4(CreateObject, uint32_t(const chromeos::SecureBlob&,
                                      uint64_t,
                                      const std::vector<uint8_t>&,
                                      uint64_t*));
  MOCK_METHOD5(CopyObject, uint32_t(const chromeos::SecureBlob&, uint64_t,
                                    uint64_t, const std::vector<uint8_t>&,
                                    uint64_t*));
  MOCK_METHOD3(DestroyObject, uint32_t(const chromeos::SecureBlob&, uint64_t,
                                       uint64_t));
  MOCK_METHOD4(GetObjectSize, uint32_t(const chromeos::SecureBlob&, uint64_t,
                                       uint64_t, uint64_t*));
  MOCK_METHOD5(GetAttributeValue, uint32_t(const chromeos::SecureBlob&,
                                           uint64_t,
                                           uint64_t,
                                           const std::vector<uint8_t>&,
                                           std::vector<uint8_t>*));
  MOCK_METHOD4(SetAttributeValue, uint32_t(const chromeos::SecureBlob&,
                                           uint64_t,
                                           uint64_t,
                                           const std::vector<uint8_t>&));
  MOCK_METHOD3(FindObjectsInit, uint32_t(const chromeos::SecureBlob&, uint64_t,
                                         const std::vector<uint8_t>&));
  MOCK_METHOD4(FindObjects, uint32_t(const chromeos::SecureBlob&, uint64_t,
                                     uint64_t, std::vector<uint64_t>*));
  MOCK_METHOD2(FindObjectsFinal, uint32_t(const chromeos::SecureBlob&,
                                          uint64_t));
  MOCK_METHOD5(EncryptInit, uint32_t(const chromeos::SecureBlob&,
                                     uint64_t,
                                     uint64_t,
                                     const std::vector<uint8_t>&,
                                     uint64_t key_handle));
  MOCK_METHOD6(Encrypt, uint32_t(const chromeos::SecureBlob&,
                                 uint64_t,
                                 const std::vector<uint8_t>&,
                                 uint64_t,
                                 uint64_t*,
                                 std::vector<uint8_t>*));
  MOCK_METHOD6(EncryptUpdate, uint32_t(const chromeos::SecureBlob&,
                                       uint64_t,
                                       const std::vector<uint8_t>&,
                                       uint64_t,
                                       uint64_t*,
                                       std::vector<uint8_t>*));
  MOCK_METHOD5(EncryptFinal, uint32_t(const chromeos::SecureBlob&,
                                      uint64_t,
                                      uint64_t,
                                      uint64_t*,
                                      std::vector<uint8_t>*));
  MOCK_METHOD2(EncryptCancel, void(const chromeos::SecureBlob&,
                                   uint64_t));
  MOCK_METHOD5(DecryptInit, uint32_t(const chromeos::SecureBlob&,
                                     uint64_t,
                                     uint64_t,
                                     const std::vector<uint8_t>&,
                                     uint64_t));
  MOCK_METHOD6(Decrypt, uint32_t(const chromeos::SecureBlob&,
                                 uint64_t,
                                 const std::vector<uint8_t>&,
                                 uint64_t,
                                 uint64_t*,
                                 std::vector<uint8_t>*));
  MOCK_METHOD6(DecryptUpdate, uint32_t(const chromeos::SecureBlob&,
                                       uint64_t,
                                       const std::vector<uint8_t>&,
                                       uint64_t,
                                       uint64_t*,
                                       std::vector<uint8_t>*));
  MOCK_METHOD5(DecryptFinal, uint32_t(const chromeos::SecureBlob&,
                                      uint64_t,
                                      uint64_t,
                                      uint64_t*,
                                      std::vector<uint8_t>*));
  MOCK_METHOD2(DecryptCancel, void(const chromeos::SecureBlob&,
                                   uint64_t));
  MOCK_METHOD4(DigestInit, uint32_t(const chromeos::SecureBlob&,
                                    uint64_t,
                                    uint64_t,
                                    const std::vector<uint8_t>&));
  MOCK_METHOD6(Digest, uint32_t(const chromeos::SecureBlob&,
                                uint64_t,
                                const std::vector<uint8_t>&,
                                uint64_t,
                                uint64_t*,
                                std::vector<uint8_t>*));
  MOCK_METHOD3(DigestUpdate, uint32_t(const chromeos::SecureBlob&,
                                      uint64_t,
                                      const std::vector<uint8_t>&));
  MOCK_METHOD3(DigestKey, uint32_t(const chromeos::SecureBlob&,
                                   uint64_t,
                                   uint64_t));
  MOCK_METHOD5(DigestFinal, uint32_t(const chromeos::SecureBlob&,
                                     uint64_t,
                                     uint64_t,
                                     uint64_t*,
                                     std::vector<uint8_t>*));
  MOCK_METHOD2(DigestCancel, void(const chromeos::SecureBlob&,
                                  uint64_t));
  MOCK_METHOD5(SignInit, uint32_t(const chromeos::SecureBlob&,
                                  uint64_t,
                                  uint64_t,
                                  const std::vector<uint8_t>&,
                                  uint64_t));
  MOCK_METHOD6(Sign, uint32_t(const chromeos::SecureBlob&,
                              uint64_t,
                              const std::vector<uint8_t>&,
                              uint64_t,
                              uint64_t*,
                              std::vector<uint8_t>*));
  MOCK_METHOD3(SignUpdate, uint32_t(const chromeos::SecureBlob&,
                                    uint64_t,
                                    const std::vector<uint8_t>&));
  MOCK_METHOD5(SignFinal, uint32_t(const chromeos::SecureBlob&,
                                   uint64_t,
                                   uint64_t,
                                   uint64_t*,
                                   std::vector<uint8_t>*));
  MOCK_METHOD2(SignCancel, void(const chromeos::SecureBlob&,
                                uint64_t));
  MOCK_METHOD5(SignRecoverInit, uint32_t(const chromeos::SecureBlob&,
                                         uint64_t,
                                         uint64_t,
                                         const std::vector<uint8_t>&,
                                         uint64_t));
  MOCK_METHOD6(SignRecover, uint32_t(const chromeos::SecureBlob&,
                                     uint64_t,
                                     const std::vector<uint8_t>&,
                                     uint64_t,
                                     uint64_t*,
                                     std::vector<uint8_t>*));
  MOCK_METHOD5(VerifyInit, uint32_t(const chromeos::SecureBlob&,
                                    uint64_t,
                                    uint64_t,
                                    const std::vector<uint8_t>&,
                                    uint64_t));
  MOCK_METHOD4(Verify, uint32_t(const chromeos::SecureBlob&,
                                uint64_t,
                                const std::vector<uint8_t>&,
                                const std::vector<uint8_t>&));
  MOCK_METHOD3(VerifyUpdate, uint32_t(const chromeos::SecureBlob&,
                                      uint64_t,
                                      const std::vector<uint8_t>&));
  MOCK_METHOD3(VerifyFinal, uint32_t(const chromeos::SecureBlob&,
                                     uint64_t,
                                     const std::vector<uint8_t>&));
  MOCK_METHOD2(VerifyCancel, void(const chromeos::SecureBlob&,
                                  uint64_t));
  MOCK_METHOD5(VerifyRecoverInit, uint32_t(const chromeos::SecureBlob&,
                                           uint64_t,
                                           uint64_t,
                                           const std::vector<uint8_t>&,
                                           uint64_t));
  MOCK_METHOD6(VerifyRecover, uint32_t(const chromeos::SecureBlob&,
                                       uint64_t,
                                       const std::vector<uint8_t>&,
                                       uint64_t,
                                       uint64_t*,
                                       std::vector<uint8_t>*));
  MOCK_METHOD6(DigestEncryptUpdate, uint32_t(const chromeos::SecureBlob&,
                                             uint64_t,
                                             const std::vector<uint8_t>&,
                                             uint64_t,
                                             uint64_t*,
                                             std::vector<uint8_t>*));
  MOCK_METHOD6(DecryptDigestUpdate, uint32_t(const chromeos::SecureBlob&,
                                             uint64_t,
                                             const std::vector<uint8_t>&,
                                             uint64_t,
                                             uint64_t*,
                                             std::vector<uint8_t>*));
  MOCK_METHOD6(SignEncryptUpdate, uint32_t(const chromeos::SecureBlob&,
                                           uint64_t,
                                           const std::vector<uint8_t>&,
                                           uint64_t,
                                           uint64_t*,
                                           std::vector<uint8_t>*));
  MOCK_METHOD6(DecryptVerifyUpdate, uint32_t(const chromeos::SecureBlob&,
                                             uint64_t,
                                             const std::vector<uint8_t>&,
                                             uint64_t,
                                             uint64_t*,
                                             std::vector<uint8_t>*));
  MOCK_METHOD6(GenerateKey, uint32_t(const chromeos::SecureBlob&,
                                     uint64_t,
                                     uint64_t,
                                     const std::vector<uint8_t>&,
                                     const std::vector<uint8_t>&,
                                     uint64_t*));
  MOCK_METHOD8(GenerateKeyPair, uint32_t(const chromeos::SecureBlob&,
                                         uint64_t,
                                         uint64_t,
                                         const std::vector<uint8_t>&,
                                         const std::vector<uint8_t>&,
                                         const std::vector<uint8_t>&,
                                         uint64_t*,
                                         uint64_t*));
  MOCK_METHOD9(WrapKey, uint32_t(const chromeos::SecureBlob&,
                                 uint64_t,
                                 uint64_t,
                                 const std::vector<uint8_t>&,
                                 uint64_t,
                                 uint64_t,
                                 uint64_t,
                                 uint64_t*,
                                 std::vector<uint8_t>*));
  MOCK_METHOD8(UnwrapKey, uint32_t(const chromeos::SecureBlob&,
                                   uint64_t,
                                   uint64_t,
                                   const std::vector<uint8_t>&,
                                   uint64_t,
                                   const std::vector<uint8_t>&,
                                   const std::vector<uint8_t>&,
                                   uint64_t*));
  MOCK_METHOD7(DeriveKey, uint32_t(const chromeos::SecureBlob&,
                                   uint64_t,
                                   uint64_t,
                                   const std::vector<uint8_t>&,
                                   uint64_t,
                                   const std::vector<uint8_t>&,
                                   uint64_t*));
  MOCK_METHOD3(SeedRandom, uint32_t(const chromeos::SecureBlob&,
                                    uint64_t,
                                    const std::vector<uint8_t>&));
  MOCK_METHOD4(GenerateRandom, uint32_t(const chromeos::SecureBlob&,
                                        uint64_t,
                                        uint64_t,
                                        std::vector<uint8_t>*));

 private:
  chromeos::SecureBlob isolate_credential_;

  DISALLOW_COPY_AND_ASSIGN(ChapsProxyMock);
};

}  // namespace chaps

#endif  // CHAPS_CHAPS_PROXY_MOCK_H_
