// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/mount_encrypted/tpm.h"

#include <stddef.h>
#include <stdint.h>

#include <base/logging.h>

#include <vboot/tlcl.h>

#include <openssl/rand.h>

#include "cryptohome/mount_encrypted.h"

Tpm::Tpm() {
#if USE_TPM2
  is_tpm2_ = true;
#endif

  VLOG(1) << "Opening TPM";

  setenv("TPM_NO_EXIT", "1", 1);
  available_ = (TlclLibInit() == TPM_SUCCESS);

  LOG(INFO) << "TPM " << (available_ ? "ready" : "not available");
}

Tpm::~Tpm() {
  if (available_) {
    TlclLibClose();
  }
}

result_code Tpm::IsOwned(bool* owned) {
  if (ownership_checked_) {
    *owned = owned_;
    return RESULT_SUCCESS;
  }

  VLOG(1) << "Reading TPM Ownership Flag";
  if (!available_) {
    return RESULT_FAIL_FATAL;
  }

  uint8_t tmp_owned = 0;
  uint32_t result = TlclGetOwnership(&tmp_owned);
  VLOG(1) << "TPM Ownership Flag returned: " << (result ? "FAIL" : "ok");
  if (result != TPM_SUCCESS) {
    LOG(INFO) << "Could not determine TPM ownership: error " << result;
    return RESULT_FAIL_FATAL;
  }

  ownership_checked_ = true;
  owned_ = tmp_owned;
  *owned = owned_;
  return RESULT_SUCCESS;
}

result_code Tpm::GetRandomBytes(uint8_t* buffer, int wanted) {
  if (available()) {
    // Read random bytes from TPM, which can return short reads.
    int remaining = wanted;
    while (remaining) {
      uint32_t result, size;
      result = TlclGetRandom(buffer + (wanted - remaining), remaining, &size);
      if (result != TPM_SUCCESS) {
        LOG(ERROR) << "TPM GetRandom failed: error " << result;
        return RESULT_FAIL_FATAL;
      }
      CHECK_LE(size, remaining);
      remaining -= size;
    }

    if (remaining == 0) {
      return RESULT_SUCCESS;
    }
  }

  // Fall back to system random source.
  if (RAND_bytes(buffer, wanted)) {
    return RESULT_SUCCESS;
  }

  LOG(ERROR) << "Failed to obtain randomness.";
  return RESULT_FAIL_FATAL;
}

uint8_t nvram_data[LOCKBOX_SIZE_MAX];
uint32_t nvram_size = 0;

static uint32_t _read_nvram(Tpm* tpm,
                            uint8_t* buffer,
                            size_t len,
                            uint32_t index,
                            uint32_t size) {
  uint32_t result;

  if (size > len) {
    ERROR("NVRAM size (0x%x > 0x%zx) is too big", size, len);
    return 0;
  }

  DEBUG("Reading NVRAM area 0x%x (size %u)", index, size);
  if (!tpm->available())
    result = TPM_E_NO_DEVICE;
  else
    result = TlclRead(index, buffer, size);
  DEBUG("NVRAM read returned: %s", result == TPM_SUCCESS ? "ok" : "FAIL");

  return result;
}

/*
 * Cache Lockbox NVRAM area in nvram_data, set nvram_size to the actual size.
 * Set *migrate to 0 for Version 2 Lockbox area, and 1 otherwise.
 */
result_code read_lockbox_nvram_area(Tpm* tpm, int* migrate) {
  uint8_t bytes_anded, bytes_ored;
  uint32_t result, i;

  /* Default to allowing migration (disallow when owned with NVRAMv2). */
  *migrate = 1;

  /* Ignore unowned TPM's NVRAM area. */
  bool owned = false;
  result_code rc = tpm->IsOwned(&owned);
  if (rc != RESULT_SUCCESS) {
    return rc;
  }
  if (!owned) {
    INFO("TPM not Owned, ignoring Lockbox NVRAM area.");
    return RESULT_FAIL_FATAL;
  }

  /* Reading the NVRAM takes 40ms. Instead of querying the NVRAM area
   * for its size (which takes time), just read the expected size. If
   * it fails, then fall back to the older size. This means cleared
   * devices take 80ms (2 failed reads), legacy devices take 80ms
   * (1 failed read, 1 good read), and populated devices take 40ms,
   * which is the minimum possible time (instead of 40ms + time to
   * query NVRAM size).
   */
  result = _read_nvram(tpm, nvram_data, sizeof(nvram_data), kLockboxIndex,
                       kLockboxSizeV2);
  if (result != TPM_SUCCESS) {
    result = _read_nvram(tpm, nvram_data, sizeof(nvram_data), kLockboxIndex,
                         kLockboxSizeV1);
    if (result != TPM_SUCCESS) {
      /* No NVRAM area at all. */
      INFO("No Lockbox NVRAM area defined: error 0x%02x", result);
      return RESULT_FAIL_FATAL;
    }
    /* Legacy NVRAM area. */
    nvram_size = kLockboxSizeV1;
    INFO("Version 1 Lockbox NVRAM area found.");
  } else {
    *migrate = 0;
    nvram_size = kLockboxSizeV2;
    INFO("Version 2 Lockbox NVRAM area found.");
  }

  debug_dump_hex("lockbox nvram", value, nvram_size);

  /* Ignore defined but unwritten NVRAM area. */
  bytes_ored = 0x0;
  bytes_anded = 0xff;
  for (i = 0; i < nvram_size; ++i) {
    bytes_ored |= nvram_data[i];
    bytes_anded &= nvram_data[i];
  }
  if (bytes_ored == 0x0 || bytes_anded == 0xff) {
    INFO("NVRAM area has been defined but not written.");
    nvram_size = 0;
    return RESULT_FAIL_FATAL;
  }

  return RESULT_SUCCESS;
}
