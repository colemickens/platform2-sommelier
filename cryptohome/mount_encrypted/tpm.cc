// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/mount_encrypted/tpm.h"

#include <stddef.h>
#include <stdint.h>

#include <vboot/tlcl.h>

#include <openssl/rand.h>

#include "cryptohome/mount_encrypted.h"

int has_tpm = 0;
uint8_t nvram_data[LOCKBOX_SIZE_MAX];
uint32_t nvram_size = 0;

static int tpm_init_called = 0;

void tpm_init() {
  uint32_t result;

  if (tpm_init_called)
    return;

  DEBUG("Opening TPM");

  setenv("TPM_NO_EXIT", "1", 1);
  result = TlclLibInit();

  tpm_init_called = 1;
  has_tpm = (result == TPM_SUCCESS);
  INFO("TPM %s", has_tpm ? "ready" : "not available");
}

/* Returns TPM result status code, and on TPM_SUCCESS, stores ownership
 * flag to "owned".
 */
uint32_t tpm_owned(uint8_t* owned) {
  uint32_t result;

  tpm_init();
  DEBUG("Reading TPM Ownership Flag");
  if (!has_tpm)
    result = TPM_E_NO_DEVICE;
  else
    result = TlclGetOwnership(owned);

  DEBUG("TPM Ownership Flag returned: %s", result ? "FAIL" : "ok");

  return result;
}

void tpm_close(void) {
  if (!has_tpm || !tpm_init_called)
    return;
  TlclLibClose();
  tpm_init_called = 0;
}

result_code get_random_bytes_tpm(unsigned char* buffer, int wanted) {
  uint32_t remaining = wanted;

  tpm_init();
  /* Read random bytes from TPM, which can return short reads. */
  while (remaining) {
    uint32_t result, size;

    result = TlclGetRandom(buffer + (wanted - remaining), remaining, &size);
    if (result != TPM_SUCCESS || size > remaining) {
      ERROR("TPM GetRandom failed: error 0x%02x.", result);
      return RESULT_FAIL_FATAL;
    }
    remaining -= size;
  }

  return RESULT_SUCCESS;
}

result_code get_random_bytes(unsigned char* buffer, int wanted) {
  if (has_tpm && get_random_bytes_tpm(buffer, wanted) == RESULT_SUCCESS)
    return RESULT_SUCCESS;

  if (RAND_bytes(buffer, wanted))
    return RESULT_SUCCESS;
  SSL_ERROR("RAND_bytes");

  return RESULT_FAIL_FATAL;
}

static uint32_t _read_nvram(uint8_t* buffer,
                            size_t len,
                            uint32_t index,
                            uint32_t size) {
  uint32_t result;

  if (size > len) {
    ERROR("NVRAM size (0x%x > 0x%zx) is too big", size, len);
    return 0;
  }

  tpm_init();
  DEBUG("Reading NVRAM area 0x%x (size %u)", index, size);
  if (!has_tpm)
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
result_code read_lockbox_nvram_area(int* migrate) {
  uint8_t owned = 0;
  uint8_t bytes_anded, bytes_ored;
  uint32_t result, i;

  /* Default to allowing migration (disallow when owned with NVRAMv2). */
  *migrate = 1;

  /* Ignore unowned TPM's NVRAM area. */
  result = tpm_owned(&owned);
  if (result != TPM_SUCCESS) {
    INFO("Could not read TPM Permanent Flags: error 0x%02x.", result);
    return RESULT_FAIL_FATAL;
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
  result = _read_nvram(nvram_data, sizeof(nvram_data), kLockboxIndex,
                       kLockboxSizeV2);
  if (result != TPM_SUCCESS) {
    result = _read_nvram(nvram_data, sizeof(nvram_data), kLockboxIndex,
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
