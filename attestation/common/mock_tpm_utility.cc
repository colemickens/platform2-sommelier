// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "attestation/common/mock_tpm_utility.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace {

bool CopyString(const std::string& in, std::string* out) {
  *out = in;
  return true;
}

}  // namespace

namespace attestation {

MockTpmUtility::MockTpmUtility() {
  ON_CALL(*this, IsTpmReady()).WillByDefault(Return(true));
  ON_CALL(*this, ActivateIdentity(_, _, _, _, _, _))
      .WillByDefault(Return(true));
  ON_CALL(*this, CreateCertifiedKey(_, _, _, _, _, _, _, _, _))
      .WillByDefault(Return(true));
  ON_CALL(*this, SealToPCR0(_, _))
      .WillByDefault(Invoke(CopyString));
  ON_CALL(*this, Unseal(_, _))
      .WillByDefault(Invoke(CopyString));
}

MockTpmUtility::~MockTpmUtility() {}

}  // namespace attestation
