// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_TPM_UTILITY_MOCK_H
#define CHAPS_TPM_UTILITY_MOCK_H

#include "chaps/tpm_utility.h"

namespace chaps {

class TPMUtilityMock : public TPMUtility {
 public:
  MOCK_METHOD1(Init, bool(const std::string&));
  MOCK_METHOD4(Authenticate, bool(const std::string&,
                                  const std::string&,
                                  const std::string&,
                                  std::string*));
  MOCK_METHOD4(ChangeAuthData, bool(const std::string&,
                                    const std::string&,
                                    const std::string&,
                                    std::string*));
  MOCK_METHOD2(GenerateRandom, bool(int, std::string*));
  MOCK_METHOD2(GenerateKey, bool(const std::string&, std::string*));
  MOCK_METHOD4(Bind, bool(const std::string&,
                          const std::string&,
                          const std::string&,
                          std::string*));
};

}  // namespace chaps

#endif  // CHAPS_TPM_UTILITY_MOCK_H
