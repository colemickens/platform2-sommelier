// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/mount_encrypted/tpm.h"

#include <stddef.h>
#include <stdint.h>

#include <openssl/sha.h>

#include <vboot/tlcl.h>

#include "cryptohome/mount_encrypted.h"

static const uint32_t kLockboxSaltOffset = 0x5;

/*
 * TPM cases:
 *  - does not exist at all (disabled in test firmware or non-chrome device).
 *  - exists (below).
 *
 * TPM ownership cases:
 *  - unowned (OOBE):
 *    - expect modern lockbox (no migration allowed).
 *  - owned: depends on NVRAM area (below).
 *
 * NVRAM area cases:
 *  - no NVRAM area at all:
 *    - interrupted install (cryptohome has the TPM password)
 *    - ancient device (cr48, cryptohome has thrown away TPM password)
 *    - broken device (cryptohome has thrown away/never had TPM password)
 *      - must expect worst-case: no lockbox ever, and migration allowed.
 *  - defined NVRAM area, but not written to ("Finalized"); interrupted OOBE:
 *    - if legacy size, allow migration.
 *    - if not, disallow migration.
 *  - written ("Finalized") NVRAM area:
 *    - if legacy size, allow migration.
 *    - if not, disallow migration.
 *
 * In case of success: (NVRAM area found and used)
 *  - *digest populated with NVRAM area entropy.
 *  - *migrate is 1 for NVRAM v1, 0 for NVRAM v2.
 * In case of failure: (NVRAM missing or error)
 *  - *digest untouched.
 *  - *migrate always 1
 */
result_code get_nvram_key(uint8_t* digest, int* migrate) {
  uint8_t* rand_bytes;
  uint32_t rand_size;
  result_code rc;

  /* Read lockbox nvram data and "export" it for use after the helper. */
  rc = read_lockbox_nvram_area(migrate);
  if (rc != RESULT_SUCCESS)
    return rc;

  /* Choose random bytes to use based on NVRAM version. */
  if (nvram_size == kLockboxSizeV1) {
    rand_bytes = nvram_data;
    rand_size = nvram_size;
  } else {
    rand_bytes = nvram_data + kLockboxSaltOffset;
    if (kLockboxSaltOffset + DIGEST_LENGTH > nvram_size) {
      INFO("Impossibly small NVRAM area size (%d).", nvram_size);
      return RESULT_FAIL_FATAL;
    }
    rand_size = DIGEST_LENGTH;
  }
  if (rand_size < DIGEST_LENGTH) {
    INFO("Impossibly small rand_size (%d).", rand_size);
    return RESULT_FAIL_FATAL;
  }
  debug_dump_hex("rand_bytes", rand_bytes, rand_size);

  SHA256(rand_bytes, rand_size, digest);
  debug_dump_hex("system key", digest, DIGEST_LENGTH);

  return RESULT_SUCCESS;
}
