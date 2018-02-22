// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test methods that run on a real TPM

#ifndef CRYPTOHOME_TPM_LIVE_TEST_H_
#define CRYPTOHOME_TPM_LIVE_TEST_H_

#include "cryptohome/tpm.h"

#include <base/logging.h>
#include <brillo/secure_blob.h>

namespace cryptohome {

class TpmLiveTest {
 public:
  TpmLiveTest();
  ~TpmLiveTest() = default;

  // This method runs all the live tests in this class, if the owner_password
  // is provided. If the owner_password is empty, this method runs
  // all the tests that do not require the owner password.
  bool RunLiveTests(brillo::SecureBlob owner_password);

 private:
  // This test checks if PCRs and PCR bound keys work correctly.
  bool PCRKeyTest();

  // This test checks if we can create and load an RSA decryption key and use
  // it to encrypt and decrypt.
  bool DecryptionKeyTest();

  // The below tests need |owner_password| to be provided to run.
  // This test verifies that the Nvram subsystem of the TPM is working
  // correctly.
  bool NvramTest();
  Tpm* tpm_;

  DISALLOW_COPY_AND_ASSIGN(TpmLiveTest);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_TPM_LIVE_TEST_H_
