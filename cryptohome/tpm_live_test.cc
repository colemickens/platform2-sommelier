// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test methods that run on a real TPM

#include "cryptohome/tpm_live_test.h"

using chromeos::SecureBlob;

namespace cryptohome {

TpmLiveTest::TpmLiveTest() : tpm_(Tpm::GetSingleton()) {}

bool TpmLiveTest::RunLiveTests(SecureBlob owner_password) {
  if (!owner_password.empty()) {
    VLOG(1) << "Running tests that require owner password.";
    tpm_->SetOwnerPassword(owner_password);
    if (!NvramTest()) {
      LOG(ERROR) << "Error running NvramTest.";
      return false;
    }
  }
  LOG(INFO) << "All tests run successfully.";
  return true;
}

bool TpmLiveTest::NvramTest() {
  uint32_t index = 12;
  SecureBlob nvram_data("nvram_data");
  if (tpm_->IsNvramDefined(index)) {
    LOG(ERROR) << "Nvram index is already defined.";
    return false;
  }
  if (!tpm_->DefineLockOnceNvram(index, nvram_data.size())) {
    LOG(ERROR) << "Defining Nvram index.";
    return false;
  }
  if (!tpm_->IsNvramDefined(index)) {
    LOG(ERROR) << "Nvram index is not defined after creating.";
    return false;
  }
  if (tpm_->GetNvramSize(index) != nvram_data.size()) {
    LOG(ERROR) << "Nvram space is of incorrect size.";
    return false;
  }
  if (tpm_->IsNvramLocked(index)) {
    LOG(ERROR) << "Nvram should not be locked before writing.";
    return false;
  }
  if (!tpm_->WriteNvram(index, nvram_data)) {
    LOG(ERROR) << "Error writing to Nvram.";
    return false;
  }
  if (!tpm_->IsNvramLocked(index)) {
    LOG(ERROR) << "Nvram should be locked after a write.";
    return false;
  }
  SecureBlob data;
  if (!tpm_->ReadNvram(index, &data)) {
    LOG(ERROR) << "Error reading from Nvram.";
    return false;
  }
  if (data != nvram_data) {
    LOG(ERROR) << "Data read from Nvram did not match data written.";
    return false;
  }
  if (tpm_->WriteNvram(index, nvram_data)) {
    LOG(ERROR) << "We should not be able to write to a locked Nvram space.";
    return false;
  }
  if (!tpm_->DestroyNvram(index)) {
    LOG(ERROR) << "Error destroying Nvram space.";
    return false;
  }
  if (tpm_->IsNvramDefined(index)) {
    LOG(ERROR) << "Nvram still defined after it was destroyed.";
    return false;
  }
  VLOG(1) << "NvramTest eneded successfully.";
  return true;
}

}  // namespace cryptohome
