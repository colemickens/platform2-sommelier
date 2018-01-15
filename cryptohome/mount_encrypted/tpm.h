// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Interface used by "mount-encrypted" to interface with the TPM.

#ifndef CRYPTOHOME_MOUNT_ENCRYPTED_TPM_H_
#define CRYPTOHOME_MOUNT_ENCRYPTED_TPM_H_

#include <stdint.h>

#define LOCKBOX_SIZE_MAX 0x45

const uint32_t kLockboxSizeV1 = 0x2c;
const uint32_t kLockboxSizeV2 = LOCKBOX_SIZE_MAX;

#if USE_TPM2
static const uint32_t kLockboxIndex = 0x800004;
#else
static const uint32_t kLockboxIndex = 0x20000004;
#endif

extern int has_tpm;
extern uint8_t nvram_data[LOCKBOX_SIZE_MAX];
extern uint32_t nvram_size;

enum result_code {
  RESULT_SUCCESS = 0,
  RESULT_FAIL_FATAL = 1,
};

// Load the encryption key from TPM NVRAM. Returns true if successful and fills
// in key, false if the key is not available or there is an error.
//
// TODO(mnissler): Remove the migration feature - Chrome OS has supported
// encrypted stateful for years, the chance that a device that was set up with a
// Chrome OS version that didn't support stateful encryption will be switching
// directly to this version of the code is negligibly small.
result_code get_nvram_key(uint8_t* digest, int* migrate);

inline int is_tpm2(void) {
#if USE_TPM2
  return 1;
#else
  return 0;
#endif
}

void tpm_init();
uint32_t tpm_owned(uint8_t* owned);
void tpm_close(void);

result_code get_random_bytes(unsigned char* buffer, int wanted);

result_code read_lockbox_nvram_area(int* migrate);

#endif  // CRYPTOHOME_MOUNT_ENCRYPTED_TPM_H_
