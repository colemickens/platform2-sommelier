// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Interface used by "mount-encrypted" to interface with the TPM.

#ifndef CRYPTOHOME_MOUNT_ENCRYPTED_TPM_H_
#define CRYPTOHOME_MOUNT_ENCRYPTED_TPM_H_

#include <stdint.h>

#include <memory>

#include <base/macros.h>

#include <brillo/secure_blob.h>

#include "cryptohome/mount_encrypted.h"

// Encapsulates high-level TPM state and the motions needed to open and close
// the TPM library.
class Tpm {
 public:
  Tpm();
  ~Tpm();

  bool available() const { return available_; }
  bool is_tpm2() const { return is_tpm2_; }

  result_code IsOwned(bool* owned);

  result_code GetRandomBytes(uint8_t* buffer, int wanted);

 private:
  bool available_ = false;
  bool is_tpm2_ = false;

  bool ownership_checked_ = false;
  bool owned_ = false;

  DISALLOW_COPY_AND_ASSIGN(Tpm);
};

#define LOCKBOX_SIZE_MAX 0x45

const uint32_t kLockboxSizeV1 = 0x2c;
const uint32_t kLockboxSizeV2 = LOCKBOX_SIZE_MAX;

#if USE_TPM2
static const uint32_t kLockboxIndex = 0x800004;
#else
static const uint32_t kLockboxIndex = 0x20000004;
#endif

extern uint8_t nvram_data[LOCKBOX_SIZE_MAX];
extern uint32_t nvram_size;

// Load the encryption key from TPM NVRAM. Returns true if successful and fills
// in key, false if the key is not available or there is an error.
//
// TODO(mnissler): Remove the migration feature - Chrome OS has supported
// encrypted stateful for years, the chance that a device that was set up with a
// Chrome OS version that didn't support stateful encryption will be switching
// directly to this version of the code is negligibly small.
result_code get_nvram_key(Tpm* tpm, uint8_t* digest, int* migrate);

result_code read_lockbox_nvram_area(Tpm* tpm, int* migrate);

#endif  // CRYPTOHOME_MOUNT_ENCRYPTED_TPM_H_
