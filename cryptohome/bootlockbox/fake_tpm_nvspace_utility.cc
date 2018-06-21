// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/bootlockbox/fake_tpm_nvspace_utility.h"

namespace cryptohome {

bool FakeTpmNVSpaceUtility::Initialize() {
  return true;
}

bool FakeTpmNVSpaceUtility::DefineNVSpace() {
  return true;
}

bool FakeTpmNVSpaceUtility::DefineNVSpaceBeforeOwned() {
  return true;
}

bool FakeTpmNVSpaceUtility::WriteNVSpace(const std::string& digest) {
  digest_ = digest;
  return true;
}

bool FakeTpmNVSpaceUtility::ReadNVSpace(std::string* digest,
                                        NVSpaceState* state) {
  *digest = digest_;
  *state = NVSpaceState::kNVSpaceNormal;
  return true;
}

bool FakeTpmNVSpaceUtility::LockNVSpace() {
  return true;
}

void FakeTpmNVSpaceUtility::SetDigest(const std::string& digest) {
  digest_ = digest;
}

}  // namespace cryptohome
