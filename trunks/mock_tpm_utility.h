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
  MOCK_METHOD0(InitializeTpm, TPM_RC());
  MOCK_METHOD1(StirRandom, TPM_RC(const std::string&));
  MOCK_METHOD2(GenerateRandom, TPM_RC(int, std::string*));
  MOCK_METHOD2(ExtendPCR, TPM_RC(int, const std::string&));
  MOCK_METHOD2(ReadPCR, TPM_RC(int, std::string*));
  MOCK_METHOD3(TakeOwnership, TPM_RC(const std::string& owner_password,
                                     const std::string& endorsement_password,
                                     const std::string& lockout_password));
  MOCK_METHOD1(CreateStorageRootKeys,
               TPM_RC(const std::string& owner_password));
  MOCK_METHOD5(AsymmetricEncrypt, TPM_RC(TPM_HANDLE,
                                         TPM_ALG_ID,
                                         TPM_ALG_ID,
                                         const std::string&,
                                         std::string*));
  MOCK_METHOD6(AsymmetricDecrypt, TPM_RC(TPM_HANDLE,
                                         TPM_ALG_ID,
                                         TPM_ALG_ID,
                                         const std::string&,
                                         const std::string&,
                                         std::string*));
  MOCK_METHOD6(Sign, TPM_RC(TPM_HANDLE,
                            TPM_ALG_ID,
                            TPM_ALG_ID,
                            const std::string&,
                            const std::string&,
                            std::string*));
  MOCK_METHOD5(Verify, TPM_RC(TPM_HANDLE,
                              TPM_ALG_ID,
                              TPM_ALG_ID,
                              const std::string&,
                              const std::string&));
};

}  // namespace trunks

#endif  // TRUNKS_MOCK_TPM_UTILITY_H_
