// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_CHAPS_PROXY_MOCK_H
#define CHAPS_CHAPS_PROXY_MOCK_H

#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "chaps/chaps_interface.h"

namespace chaps {

// defined in chaps.cc
extern void EnableMockProxy(ChapsInterface* proxy, bool is_initialized);
extern void DisableMockProxy();

// ChapsProxyMock is a mock of ChapsInterface
class ChapsProxyMock : public ChapsInterface {
public:
  ChapsProxyMock(bool is_initialized);
  virtual ~ChapsProxyMock();

  MOCK_METHOD2(GetSlotList, uint32_t (bool, std::vector<uint64_t>*));
  MOCK_METHOD8(GetSlotInfo, uint32_t (uint64_t,
                                      std::vector<uint8_t>*,
                                      std::vector<uint8_t>*,
                                      uint64_t*,
                                      uint8_t*, uint8_t*,
                                      uint8_t*, uint8_t*));
  virtual uint32_t GetTokenInfo(uint64_t slot_id,
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
  MOCK_METHOD2(GetMechanismList, uint32_t (uint64_t, std::vector<uint64_t>*));
  MOCK_METHOD5(GetMechanismInfo, uint32_t (uint64_t, uint64_t, uint64_t*,
                                           uint64_t*, uint64_t*));
  MOCK_METHOD3(InitToken, uint32_t (uint64_t, const std::string*,
                                    const std::vector<uint8_t>&));
  MOCK_METHOD2(InitPIN, uint32_t (uint64_t, const std::string*));
  MOCK_METHOD3(SetPIN, uint32_t (uint64_t, const std::string*,
                                 const std::string*));
  MOCK_METHOD3(OpenSession, uint32_t (uint64_t, uint64_t, uint64_t*));
  MOCK_METHOD1(CloseSession, uint32_t (uint64_t));
  MOCK_METHOD1(CloseAllSessions, uint32_t (uint64_t));
  MOCK_METHOD5(GetSessionInfo, uint32_t (uint64_t, uint64_t*, uint64_t*,
                                         uint64_t*, uint64_t*));
  MOCK_METHOD2(GetOperationState, uint32_t (uint64_t, std::vector<uint8_t>*));
  MOCK_METHOD4(SetOperationState, uint32_t (uint64_t,
                                            const std::vector<uint8_t>&,
                                            uint64_t,
                                            uint64_t));
  MOCK_METHOD3(Login, uint32_t (uint64_t, uint64_t, const std::string*));
  MOCK_METHOD1(Logout, uint32_t (uint64_t));
  MOCK_METHOD3(CreateObject, uint32_t (uint64_t, const std::vector<uint8_t>&,
                                       uint64_t*));
  MOCK_METHOD4(CopyObject, uint32_t (uint64_t, uint64_t,
                                     const std::vector<uint8_t>&, uint64_t*));
  MOCK_METHOD2(DestroyObject, uint32_t (uint64_t, uint64_t));
  MOCK_METHOD3(GetObjectSize, uint32_t (uint64_t, uint64_t, uint64_t*));
  MOCK_METHOD4(GetAttributeValue, uint32_t (uint64_t, uint64_t,
                                            const std::vector<uint8_t>&,
                                            std::vector<uint8_t>*));
  MOCK_METHOD3(SetAttributeValue, uint32_t (uint64_t, uint64_t,
                                            const std::vector<uint8_t>&));
  MOCK_METHOD2(FindObjectsInit, uint32_t (uint64_t,
                                          const std::vector<uint8_t>&));
  MOCK_METHOD3(FindObjects, uint32_t (uint64_t, uint64_t,
                                      std::vector<uint64_t>*));
  MOCK_METHOD1(FindObjectsFinal, uint32_t (uint64_t));
  MOCK_METHOD4(EncryptInit, uint32_t (uint64_t,
                               uint64_t,
                               const std::vector<uint8_t>&,
                               uint64_t key_handle));
  MOCK_METHOD5(Encrypt, uint32_t (uint64_t,
                                   const std::vector<uint8_t>&,
                                   uint64_t,
                                   uint64_t*,
                                   std::vector<uint8_t>*));
  MOCK_METHOD5(EncryptUpdate, uint32_t (uint64_t,
                                 const std::vector<uint8_t>&,
                                 uint64_t,
                                 uint64_t*,
                                 std::vector<uint8_t>*));
  MOCK_METHOD4(EncryptFinal, uint32_t (uint64_t,
                                uint64_t,
                                uint64_t*,
                                std::vector<uint8_t>*));
  MOCK_METHOD4(DecryptInit, uint32_t (uint64_t,
                               uint64_t,
                               const std::vector<uint8_t>&,
                               uint64_t));
  MOCK_METHOD5(Decrypt, uint32_t (uint64_t,
                           const std::vector<uint8_t>&,
                           uint64_t,
                           uint64_t*,
                           std::vector<uint8_t>*));
  MOCK_METHOD5(DecryptUpdate, uint32_t (uint64_t,
                                 const std::vector<uint8_t>&,
                                 uint64_t,
                                 uint64_t*,
                                 std::vector<uint8_t>*));
  MOCK_METHOD4(DecryptFinal, uint32_t (uint64_t,
                                uint64_t,
                                uint64_t*,
                                std::vector<uint8_t>*));
  MOCK_METHOD3(DigestInit, uint32_t (uint64_t,
                                     uint64_t,
                                     const std::vector<uint8_t>&));
  MOCK_METHOD5(Digest, uint32_t (uint64_t,
                                 const std::vector<uint8_t>&,
                                 uint64_t,
                                 uint64_t*,
                                 std::vector<uint8_t>*));
  MOCK_METHOD2(DigestUpdate, uint32_t (uint64_t,
                                       const std::vector<uint8_t>&));
  MOCK_METHOD2(DigestKey, uint32_t (uint64_t,
                                    uint64_t));
  MOCK_METHOD4(DigestFinal, uint32_t (uint64_t,
                                      uint64_t,
                                      uint64_t*,
                                      std::vector<uint8_t>*));
  MOCK_METHOD4(SignInit, uint32_t (uint64_t,
                                   uint64_t,
                                   const std::vector<uint8_t>&,
                                   uint64_t));
  MOCK_METHOD5(Sign, uint32_t (uint64_t,
                               const std::vector<uint8_t>&,
                               uint64_t,
                               uint64_t*,
                               std::vector<uint8_t>*));
  MOCK_METHOD2(SignUpdate, uint32_t (uint64_t,
                                     const std::vector<uint8_t>&));
  MOCK_METHOD4(SignFinal, uint32_t (uint64_t,
                                    uint64_t,
                                    uint64_t*,
                                    std::vector<uint8_t>*));
  MOCK_METHOD4(SignRecoverInit, uint32_t (uint64_t,
                                          uint64_t,
                                          const std::vector<uint8_t>&,
                                          uint64_t));
  MOCK_METHOD5(SignRecover, uint32_t (uint64_t,
                                      const std::vector<uint8_t>&,
                                      uint64_t,
                                      uint64_t*,
                                      std::vector<uint8_t>*));
  MOCK_METHOD4(VerifyInit, uint32_t (uint64_t,
                                     uint64_t,
                                     const std::vector<uint8_t>&,
                                     uint64_t));
  MOCK_METHOD3(Verify, uint32_t (uint64_t,
                                 const std::vector<uint8_t>&,
                                 const std::vector<uint8_t>&));
  MOCK_METHOD2(VerifyUpdate, uint32_t (uint64_t,
                                       const std::vector<uint8_t>&));
  MOCK_METHOD2(VerifyFinal, uint32_t (uint64_t,
                               const std::vector<uint8_t>&));
  MOCK_METHOD4(VerifyRecoverInit, uint32_t (uint64_t,
                                            uint64_t,
                                            const std::vector<uint8_t>&,
                                            uint64_t));
  MOCK_METHOD5(VerifyRecover, uint32_t (uint64_t,
                                        const std::vector<uint8_t>&,
                                        uint64_t,
                                        uint64_t*,
                                        std::vector<uint8_t>*));
  MOCK_METHOD5(DigestEncryptUpdate, uint32_t (uint64_t,
                                              const std::vector<uint8_t>&,
                                              uint64_t,
                                              uint64_t*,
                                              std::vector<uint8_t>*));
  MOCK_METHOD5(DecryptDigestUpdate, uint32_t (uint64_t,
                                              const std::vector<uint8_t>&,
                                              uint64_t,
                                              uint64_t*,
                                              std::vector<uint8_t>*));
  MOCK_METHOD5(SignEncryptUpdate, uint32_t (uint64_t,
                                            const std::vector<uint8_t>&,
                                            uint64_t,
                                            uint64_t*,
                                            std::vector<uint8_t>*));
  MOCK_METHOD5(DecryptVerifyUpdate, uint32_t (uint64_t,
                                              const std::vector<uint8_t>&,
                                              uint64_t,
                                              uint64_t*,
                                              std::vector<uint8_t>*));
  MOCK_METHOD5(GenerateKey, uint32_t (uint64_t,
                                      uint64_t,
                                      const std::vector<uint8_t>&,
                                      const std::vector<uint8_t>&,
                                      uint64_t*));
  MOCK_METHOD7(GenerateKeyPair, uint32_t (uint64_t,
                                          uint64_t,
                                          const std::vector<uint8_t>&,
                                          const std::vector<uint8_t>&,
                                          const std::vector<uint8_t>&,
                                          uint64_t*,
                                          uint64_t*));
  MOCK_METHOD8(WrapKey, uint32_t (uint64_t,
                                  uint64_t,
                                  const std::vector<uint8_t>&,
                                  uint64_t,
                                  uint64_t,
                                  uint64_t,
                                  uint64_t*,
                                  std::vector<uint8_t>*));
  MOCK_METHOD7(UnwrapKey, uint32_t (uint64_t,
                                    uint64_t,
                                    const std::vector<uint8_t>&,
                                    uint64_t,
                                    const std::vector<uint8_t>&,
                                    const std::vector<uint8_t>&,
                                    uint64_t*));
  MOCK_METHOD6(DeriveKey, uint32_t (uint64_t,
                                    uint64_t,
                                    const std::vector<uint8_t>&,
                                    uint64_t,
                                    const std::vector<uint8_t>&,
                                    uint64_t*));
  MOCK_METHOD2(SeedRandom, uint32_t (uint64_t, const std::vector<uint8_t>&));
  MOCK_METHOD3(GenerateRandom, uint32_t (uint64_t,
                                         uint64_t,
                                         std::vector<uint8_t>*));

private:
  DISALLOW_COPY_AND_ASSIGN(ChapsProxyMock);
};

}  // namespace

#endif  // CHAPS_CHAPS_PROXY_MOCK_H
