// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/mount_encrypted/tpm.h"

#include <stddef.h>
#include <stdint.h>

#include <base/logging.h>
#include <base/strings/string_number_conversions.h>

#include "cryptohome/cryptolib.h"
#include "cryptohome/mount_encrypted.h"

namespace {

const uint32_t kLockboxSaltOffset = 0x5;

}  // namespace

// TPM cases:
//  - does not exist at all (disabled in test firmware or non-chrome device).
//  - exists (below).
//
// TPM ownership cases:
//  - unowned (OOBE):
//    - expect modern lockbox (no migration allowed).
//  - owned: depends on NVRAM area (below).
//
// NVRAM area cases:
//  - no NVRAM area at all:
//    - interrupted install (cryptohome has the TPM password)
//    - ancient device (cr48, cryptohome has thrown away TPM password)
//    - broken device (cryptohome has thrown away/never had TPM password)
//      - must expect worst-case: no lockbox ever, and migration allowed.
//  - defined NVRAM area, but not written to ("Finalized"); interrupted OOBE:
//    - if legacy size, allow migration.
//    - if not, disallow migration.
//  - written ("Finalized") NVRAM area:
//    - if legacy size, allow migration.
//    - if not, disallow migration.
//
// In case of success: (NVRAM area found and used)
//  - *digest populated with NVRAM area entropy.
//  - *migrate is true for NVRAM v1, false for NVRAM v2.
// In case of failure: (NVRAM missing or error)
//  - *digest untouched.
//  - *migrate always true
result_code LoadSystemKey(Tpm* tpm,
                          brillo::SecureBlob* system_key,
                          bool* migrate) {
  *migrate = true;

  // Ignore unowned TPM's NVRAM area.
  bool owned = false;
  result_code rc = tpm->IsOwned(&owned);
  if (rc != RESULT_SUCCESS) {
    return rc;
  }
  if (!owned) {
    LOG(INFO) << "TPM not Owned, ignoring Lockbox NVRAM area.";
    return RESULT_FAIL_FATAL;
  }

  brillo::SecureBlob key_material;
  NvramSpace* lockbox_space = tpm->GetLockboxSpace();
  const brillo::SecureBlob& lockbox_contents = lockbox_space->contents();
  if (!lockbox_space->is_valid()) {
    return RESULT_FAIL_FATAL;
  } else if (lockbox_contents.size() == kLockboxSizeV1) {
    key_material = lockbox_contents;
  } else if (kLockboxSaltOffset + DIGEST_LENGTH <= lockbox_contents.size()) {
    *migrate = false;
    const uint8_t* begin = lockbox_contents.data() + kLockboxSaltOffset;
    key_material.assign(begin, begin + DIGEST_LENGTH);
  } else {
    LOG(INFO) << "Impossibly small NVRAM area size ("
              << lockbox_contents.size() << ").";
    return RESULT_FAIL_FATAL;
  }

  VLOG(1) << "rand_bytes "
          << base::HexEncode(key_material.data(), key_material.size());
  *system_key = cryptohome::CryptoLib::Sha256(key_material);
  VLOG(1) << "system_key "
          << base::HexEncode(system_key->data(), system_key->size());

  return RESULT_SUCCESS;
}
