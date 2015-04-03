// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "attestation/server/mock_tpm_utility.h"

using ::testing::_;
using ::testing::Return;

namespace attestation {

MockTpmUtility::MockTpmUtility() {
  ON_CALL(*this, IsTpmReady()).WillByDefault(Return(true));
  ON_CALL(*this, ActivateIdentity(_, _, _, _, _, _))
      .WillByDefault(Return(true));
  ON_CALL(*this, GenerateKey(_, _, _, _)).WillByDefault(Return(true));
  ON_CALL(*this, CertifyKey(_, _, _, _, _, _)).WillByDefault(Return(true));
}

MockTpmUtility::~MockTpmUtility() {}

}  // namespace attestation
