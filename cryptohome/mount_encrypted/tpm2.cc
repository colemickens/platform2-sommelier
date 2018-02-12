// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/mount_encrypted/tpm.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <vboot/tlcl.h>

#include <openssl/rand.h>

#include "cryptohome/mount_encrypted.h"

/* TPM2 NVRAM area and related constants. */
static const uint32_t kNvramAreaTpm2Index = 0x800005;
static const uint32_t kNvramAreaTpm2Magic = 0x54504D32;
static const uint32_t kNvramAreaTpm2VersionMask = 0x000000FF;
static const uint32_t kNvramAreaTpm2CurrentVersion = 1;

struct nvram_area_tpm2 {
  uint32_t magic;
  uint32_t ver_flags;
  uint8_t salt[DIGEST_LENGTH];
};

static int check_tpm_result(uint32_t result, const char* operation) {
  INFO("%s %s.", operation, result == TPM_SUCCESS ? "succeeded" : "failed");
  return result == TPM_SUCCESS;
}
#define RETURN_ON_FAILURE(x, y) \
  if (!check_tpm_result(x, y))  \
    return RESULT_FAIL_FATAL
#define LOG_RESULT(x, y) check_tpm_result(x, y)

/*
 * For TPM2, NVRAM area is separate from Lockbox.
 * No legacy systems, so migration is never allowed.
 * Cases:
 *  - wrong-size NVRAM or invalid write-locked NVRAM: tampered with / corrupted
 *    ignore
 *    will never have the salt in NVRAM (finalization_needed forever)
 *    return FAIL_FATAL (will re-create the mounts, if existed)
 *  - read-locked NVRAM: already started / tampered with
 *    ignore
 *    return FAIL_FATAL (will re-create the mounts, if existed)
 *  - no NVRAM or invalid but not write-locked NVRAM: OOBE or interrupted OOBE
 *    generate new salt, write to NVRAM, write-lock, read-lock
 *    return SUCCESS
 *  - valid NVRAM not write-locked: interrupted OOBE
 *    use NVRAM, write-lock, read-lock
 *    (security-wise not worse than finalization_needed forever)
 *    return SUCCESS
 *  - valid NVRAM:
 *    use NVRAM, read-lock
 *    return SUCCESS
 *
 * In case of success: (NVRAM area found and used)
 *  - *digest populated with NVRAM area entropy.
 *  - *migrate is 0
 * In case of failure: (NVRAM missing or error)
 *  - *digest untouched.
 *  - *migrate is 0
 */
result_code get_nvram_key(Tpm* tpm, uint8_t* digest, int* migrate) {
  uint32_t result;
  uint32_t perm;
  struct nvram_area_tpm2 area;

  /* "Export" lockbox nvram data for use after the helper.
   * Don't ever allow migration, we have no legacy TPM2 systems.
   */
  read_lockbox_nvram_area(tpm, migrate);
  *migrate = 0;

  INFO("Getting key from TPM2 NVRAM index 0x%x", kNvramAreaTpm2Index);

  if (!tpm->available())
    return RESULT_FAIL_FATAL;

  result = TlclGetPermissions(kNvramAreaTpm2Index, &perm);
  if (result == TPM_SUCCESS) {
    DEBUG("NVRAM area permissions = 0x%08x", perm);
    DEBUG("Reading %d bytes from NVRAM", sizeof(area));
    RETURN_ON_FAILURE(TlclRead(kNvramAreaTpm2Index, &area, sizeof(area)),
                      "Reading NVRAM area");
    debug_dump_hex("key nvram", reinterpret_cast<uint8_t*>(&area),
                   sizeof(area));
  } else {
    INFO("NVRAM area doesn't exist or can't check permissions");
    memset(&area, 0, sizeof(area));
    perm = TPMA_NV_AUTHWRITE | TPMA_NV_AUTHREAD | TPMA_NV_WRITEDEFINE |
           TPMA_NV_READ_STCLEAR;
    DEBUG("Defining NVRAM area 0x%x, perm 0x%08x, size %d", kNvramAreaTpm2Index,
          perm, sizeof(area));
    RETURN_ON_FAILURE(TlclDefineSpace(kNvramAreaTpm2Index, perm, sizeof(area)),
                      "Defining NVRAM area");
  }

  if (area.magic != kNvramAreaTpm2Magic ||
      (area.ver_flags & kNvramAreaTpm2VersionMask) !=
          kNvramAreaTpm2CurrentVersion) {
    unsigned char rand_bytes[DIGEST_LENGTH];
    if (perm & TPMA_NV_WRITELOCKED) {
      ERROR("NVRAM area is not valid and write-locked");
      return RESULT_FAIL_FATAL;
    }
    INFO("NVRAM area is new or not valid -- generating new key");

    if (tpm->GetRandomBytes(rand_bytes, sizeof(rand_bytes)) != RESULT_SUCCESS)
      ERROR(
          "No entropy source found -- "
          "using uninitialized stack");

    area.magic = kNvramAreaTpm2Magic;
    area.ver_flags = kNvramAreaTpm2CurrentVersion;
    memcpy(area.salt, rand_bytes, DIGEST_LENGTH);
    debug_dump_hex("key nvram", reinterpret_cast<uint8_t*>(&area),
                   sizeof(area));

    RETURN_ON_FAILURE(TlclWrite(kNvramAreaTpm2Index, &area, sizeof(area)),
                      "Writing NVRAM area");
  }

  /* Lock the area as needed. Write-lock may be already set.
   * Read-lock is never set at this point, since we were able to read.
   * Not being able to lock is not fatal, though exposes the key.
   */
  if (!(perm & TPMA_NV_WRITELOCKED))
    LOG_RESULT(TlclWriteLock(kNvramAreaTpm2Index), "Write-locking NVRAM area");
  LOG_RESULT(TlclReadLock(kNvramAreaTpm2Index), "Read-locking NVRAM area");

  /* Use the salt from the area to generate the key. */
  SHA256(area.salt, DIGEST_LENGTH, digest);
  debug_dump_hex("system key", digest, DIGEST_LENGTH);

  return RESULT_SUCCESS;
}
