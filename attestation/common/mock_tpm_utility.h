// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATTESTATION_COMMON_MOCK_TPM_UTILITY_H_
#define ATTESTATION_COMMON_MOCK_TPM_UTILITY_H_

#include "attestation/common/tpm_utility.h"

#include <string>

#include <gmock/gmock.h>

namespace attestation {

class MockTpmUtility : public TpmUtility {
 public:
  MockTpmUtility();
  ~MockTpmUtility() override;
  // By default this class will fake seal/unbind/sign operations by passing the
  // input through Transform(<method>). E.g. The expected output of a fake Sign
  // operation on "foo" can be computed by calling
  // MockTpmUtility::Transform("Sign", "foo").
  static std::string Transform(const std::string& method,
                               const std::string& input);

  MOCK_METHOD0(IsTpmReady, bool());
  MOCK_METHOD6(ActivateIdentity, bool(const std::string&,
                                      const std::string&,
                                      const std::string&,
                                      const std::string&,
                                      const std::string&,
                                      std::string*));
  MOCK_METHOD9(CreateCertifiedKey, bool(KeyType,
                                        KeyUsage,
                                        const std::string&,
                                        const std::string&,
                                        std::string*,
                                        std::string*,
                                        std::string*,
                                        std::string*,
                                        std::string*));
  MOCK_METHOD2(SealToPCR0, bool(const std::string&, std::string*));
  MOCK_METHOD2(Unseal, bool(const std::string&, std::string*));
  MOCK_METHOD1(GetEndorsementPublicKey, bool(std::string*));
  MOCK_METHOD3(Unbind, bool(const std::string&, const std::string&,
                            std::string*));
  MOCK_METHOD3(Sign, bool(const std::string&, const std::string&,
                          std::string*));
};

}  // namespace attestation

#endif  // ATTESTATION_COMMON_MOCK_TPM_UTILITY_H_
