// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remote_attestation.h"

#include <gtest/gtest.h>

namespace cryptohome {

TEST(RemoteAttestationTest, NullTpm) {
  RemoteAttestation without_tpm(NULL);
  without_tpm.Initialize();
  EXPECT_FALSE(without_tpm.IsInitialized());
}

}

