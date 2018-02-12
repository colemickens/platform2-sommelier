// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/mount_encrypted/tpm.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <base/logging.h>
#include <base/strings/string_number_conversions.h>

#include <vboot/tlcl.h>

#include <openssl/rand.h>

#include "cryptohome/cryptolib.h"
#include "cryptohome/mount_encrypted.h"

namespace {

// TPM2 NVRAM area and related constants.
const uint32_t kNvramAreaTpm2Index = 0x800005;
const uint32_t kNvramAreaTpm2Magic = 0x54504D32;
const uint32_t kNvramAreaTpm2VersionMask = 0x000000FF;
const uint32_t kNvramAreaTpm2CurrentVersion = 1;

struct nvram_area_tpm2 {
  uint32_t magic;
  uint32_t ver_flags;
  uint8_t key_material[DIGEST_LENGTH];
};

}  // namespace

// For TPM2, NVRAM area is separate from Lockbox.
// No legacy systems, so migration is never allowed.
// Cases:
//  - wrong-size NVRAM or invalid write-locked NVRAM: tampered with / corrupted
//    ignore
//    will never have the salt in NVRAM (finalization_needed forever)
//    return FAIL_FATAL (will re-create the mounts, if existed)
//  - read-locked NVRAM: already started / tampered with
//    ignore
//    return FAIL_FATAL (will re-create the mounts, if existed)
//  - no NVRAM or invalid but not write-locked NVRAM: OOBE or interrupted OOBE
//    generate new salt, write to NVRAM, write-lock, read-lock
//    return SUCCESS
//  - valid NVRAM not write-locked: interrupted OOBE
//    use NVRAM, write-lock, read-lock
//    (security-wise not worse than finalization_needed forever)
//    return SUCCESS
//  - valid NVRAM:
//    use NVRAM, read-lock
//    return SUCCESS
//
// In case of success: (NVRAM area found and used)
//  - *digest populated with NVRAM area entropy.
//  - *migrate is false
// In case of failure: (NVRAM missing or error)
//  - *digest untouched.
//  - *migrate is false
result_code LoadSystemKey(Tpm* tpm,
                          brillo::SecureBlob* system_key,
                          bool* migrate) {
  *migrate = false;

  LOG(INFO) << "Getting key from TPM2 NVRAM index " << kNvramAreaTpm2Index;

  if (!tpm->available()) {
    return RESULT_FAIL_FATAL;
  }

  const struct nvram_area_tpm2* area = nullptr;
  uint32_t attributes = 0;
  NvramSpace* encstateful_space = tpm->GetEncStatefulSpace();
  if (encstateful_space->is_valid() &&
      encstateful_space->GetAttributes(&attributes) == RESULT_SUCCESS &&
      encstateful_space->contents().size() >= sizeof(struct nvram_area_tpm2)) {
    area = reinterpret_cast<const struct nvram_area_tpm2*>(
        encstateful_space->contents().data());
  } else {
    LOG(INFO) << "NVRAM area doesn't exist or can't check attributes";
    attributes = TPMA_NV_AUTHWRITE | TPMA_NV_AUTHREAD | TPMA_NV_WRITEDEFINE |
           TPMA_NV_READ_STCLEAR;
    result_code rc =
        encstateful_space->Define(attributes, sizeof(struct nvram_area_tpm2));
    if (rc != TPM_SUCCESS) {
      LOG(ERROR) << "Failed to define NVRAM space.";
      return rc;
    }
  }

  if (!area || area->magic != kNvramAreaTpm2Magic ||
      (area->ver_flags & kNvramAreaTpm2VersionMask) !=
          kNvramAreaTpm2CurrentVersion) {
    if (attributes & TPMA_NV_WRITELOCKED) {
      LOG(ERROR) << "NVRAM area is not valid and write-locked";
      return RESULT_FAIL_FATAL;
    }

    LOG(INFO) << "NVRAM area is new or not valid -- generating new key";

    brillo::SecureBlob new_contents(sizeof(struct nvram_area_tpm2));
    struct nvram_area_tpm2* new_area =
        reinterpret_cast<struct nvram_area_tpm2*>(new_contents.data());
    new_area->magic = kNvramAreaTpm2Magic;
    new_area->ver_flags = kNvramAreaTpm2CurrentVersion;
    cryptohome::CryptoLib::GetSecureRandom(new_area->key_material,
                                           sizeof(new_area->key_material));

    VLOG(1) << "key nvram "
            << base::HexEncode(new_contents.data(), new_contents.size());

    result_code rc = encstateful_space->Write(new_contents);
    if (rc != RESULT_SUCCESS) {
      LOG(ERROR) << "Failed to write NVRAM area";
      return rc;
    }

    area = new_area;
  }

  // Lock the area as needed. Write-lock may be already set.
  // Read-lock is never set at this point, since we were able to read.
  // Not being able to lock is not fatal, though exposes the key.
  if (!(attributes & TPMA_NV_WRITELOCKED) &&
      encstateful_space->WriteLock() != RESULT_SUCCESS) {
    LOG(ERROR) << "Failed to write-lock NVRAM area.";
  }
  if (encstateful_space->ReadLock() != RESULT_SUCCESS) {
    LOG(ERROR) << "Failed to read-lock NVRAM area.";
  }

  // Derive the system key from the key material.
  *system_key = cryptohome::CryptoLib::Sha256(brillo::SecureBlob(
      area->key_material, area->key_material + sizeof(area->key_material)));

  VLOG(1) << "system key "
          << base::HexEncode(system_key->data(), system_key->size());

  return RESULT_SUCCESS;
}
