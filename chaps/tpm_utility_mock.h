// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_TPM_UTILITY_MOCK_H
#define CHAPS_TPM_UTILITY_MOCK_H

#include "chaps/tpm_utility.h"

#include <base/basictypes.h>
#include <gmock/gmock.h>

namespace chaps {

class TPMUtilityMock : public TPMUtility {
 public:
  TPMUtilityMock();
  virtual ~TPMUtilityMock();

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
  MOCK_METHOD1(StirRandom, bool(const std::string&));
  MOCK_METHOD6(GenerateKey, bool(int,
                                 int,
                                 const std::string&,
                                 const std::string&,
                                 std::string*,
                                 int*));
  MOCK_METHOD3(GetPublicKey, bool(int, std::string*, std::string*));
  MOCK_METHOD7(WrapKey, bool(int,
                             const std::string&,
                             const std::string&,
                             const std::string&,
                             const std::string&,
                             std::string*,
                             int*));
  MOCK_METHOD4(LoadKey, bool(int, const std::string&, const std::string&,
                             int*));
  MOCK_METHOD4(LoadPublicKey, bool(int, const std::string&, const std::string&,
                                   int*));
  MOCK_METHOD1(UnloadKeysForSlot, void(int));
  MOCK_METHOD3(Bind, bool(int, const std::string&, std::string*));
  MOCK_METHOD3(Unbind, bool(int, const std::string&, std::string*));
  MOCK_METHOD3(Sign, bool(int, const std::string&, std::string*));
  MOCK_METHOD3(Verify, bool(int, const std::string&, const std::string&));

 private:
  DISALLOW_COPY_AND_ASSIGN(TPMUtilityMock);
};

}  // namespace chaps

#endif  // CHAPS_TPM_UTILITY_MOCK_H
