// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TRUNKS_MOCK_TPM_UTILITY_H_
#define TRUNKS_MOCK_TPM_UTILITY_H_

#include "trunks/tpm_utility.h"

#include <string>

#include <gmock/gmock.h>

namespace trunks {

class MockTpmUtility : public TpmUtility {
 public:
  MockTpmUtility();
  virtual ~MockTpmUtility();

  MOCK_METHOD0(Startup, TPM_RC());
  MOCK_METHOD0(Clear, TPM_RC());
  MOCK_METHOD0(Shutdown, void());
  MOCK_METHOD0(InitializeTpm, TPM_RC());
  MOCK_METHOD3(TakeOwnership, TPM_RC(const std::string& owner_password,
                                     const std::string& endorsement_password,
                                     const std::string& lockout_password));
  MOCK_METHOD1(StirRandom, TPM_RC(const std::string&));
  MOCK_METHOD2(GenerateRandom, TPM_RC(size_t, std::string*));
  MOCK_METHOD2(ExtendPCR, TPM_RC(int, const std::string&));
  MOCK_METHOD2(ReadPCR, TPM_RC(int, std::string*));
  MOCK_METHOD5(AsymmetricEncrypt, TPM_RC(TPM_HANDLE,
                                         TPM_ALG_ID,
                                         TPM_ALG_ID,
                                         const std::string&,
                                         std::string*));
  MOCK_METHOD7(AsymmetricDecrypt, TPM_RC(TPM_HANDLE,
                                         TPM_ALG_ID,
                                         TPM_ALG_ID,
                                         const std::string&,
                                         const std::string&,
                                         AuthorizationSession*,
                                         std::string*));
  MOCK_METHOD7(Sign, TPM_RC(TPM_HANDLE,
                            TPM_ALG_ID,
                            TPM_ALG_ID,
                            const std::string&,
                            const std::string&,
                            AuthorizationSession*,
                            std::string*));
  MOCK_METHOD5(Verify, TPM_RC(TPM_HANDLE,
                              TPM_ALG_ID,
                              TPM_ALG_ID,
                              const std::string&,
                              const std::string&));
  MOCK_METHOD5(ChangeKeyAuthorizationData, TPM_RC(TPM_HANDLE,
                                                  const std::string&,
                                                  const std::string&,
                                                  AuthorizationSession*,
                                                  std::string*));
  MOCK_METHOD7(ImportRSAKey, TPM_RC(AsymmetricKeyUsage,
                                    const std::string&,
                                    uint32_t,
                                    const std::string&,
                                    const std::string&,
                                    AuthorizationSession*,
                                    std::string*));
  MOCK_METHOD5(CreateAndLoadRSAKey, TPM_RC(AsymmetricKeyUsage,
                                           const std::string&,
                                           AuthorizationSession*,
                                           TPM_HANDLE*,
                                           std::string*));
  MOCK_METHOD6(CreateRSAKeyPair, TPM_RC(AsymmetricKeyUsage,
                                        int,
                                        uint32_t,
                                        const std::string&,
                                        AuthorizationSession*,
                                        std::string*));
  MOCK_METHOD3(LoadKey, TPM_RC(const std::string&,
                               AuthorizationSession*,
                               TPM_HANDLE*));
  MOCK_METHOD2(GetKeyName, TPM_RC(TPM_HANDLE, std::string*));
  MOCK_METHOD2(GetKeyPublicArea, TPM_RC(TPM_HANDLE, TPM2B_PUBLIC*));
  MOCK_METHOD3(DefineNVSpace, TPM_RC(uint32_t,
                                     size_t,
                                     AuthorizationSession*));
  MOCK_METHOD2(DestroyNVSpace, TPM_RC(uint32_t,
                                      AuthorizationSession*));
  MOCK_METHOD2(LockNVSpace, TPM_RC(uint32_t,
                                   AuthorizationSession*));
  MOCK_METHOD4(WriteNVSpace, TPM_RC(uint32_t,
                                    uint32_t,
                                    const std::string&,
                                    AuthorizationSession*));
  MOCK_METHOD5(ReadNVSpace, TPM_RC(uint32_t,
                                   uint32_t,
                                   size_t,
                                   std::string*,
                                   AuthorizationSession*));
  MOCK_METHOD2(GetNVSpaceName, TPM_RC(uint32_t, std::string*));
  MOCK_METHOD2(GetNVSpacePublicArea, TPM_RC(uint32_t, TPMS_NV_PUBLIC*));
};

}  // namespace trunks

#endif  // TRUNKS_MOCK_TPM_UTILITY_H_
