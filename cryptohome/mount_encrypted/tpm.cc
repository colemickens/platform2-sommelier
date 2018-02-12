// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/mount_encrypted/tpm.h"

#include <stddef.h>
#include <stdint.h>

#include <base/logging.h>
#include <base/strings/string_number_conversions.h>

#include <vboot/tlcl.h>

#include <openssl/rand.h>

#include "cryptohome/mount_encrypted.h"

NvramSpace::NvramSpace(Tpm* tpm, uint32_t index)
  : tpm_(tpm), index_(index) {}

result_code NvramSpace::GetAttributes(uint32_t* attributes) {
  if (attributes_ != 0) {
    *attributes = attributes_;
    return RESULT_SUCCESS;
  }

  if (!tpm_->available()) {
    return RESULT_FAIL_FATAL;
  }

  uint32_t result = TlclGetPermissions(index_, &attributes_);
  if (result != TPM_SUCCESS) {
    LOG(ERROR) << "Failed to read NVRAM space attributes for index" << index_
               << ": " << result;
    return RESULT_FAIL_FATAL;
  }

  *attributes = attributes_;
  return RESULT_SUCCESS;
}

result_code NvramSpace::Read(uint32_t size) {
  status_ = Status::kUnknown;
  attributes_ = 0;
  contents_.clear();

  VLOG(1) << "Reading NVRAM area " << index_ << " (size " << size << ")";

  if (!tpm_->available()) {
    status_ = Status::kAbsent;
    return RESULT_FAIL_FATAL;
  }

  brillo::SecureBlob buffer(size);
  uint32_t result = TlclRead(index_, buffer.data(), buffer.size());

  VLOG(1) << "NVRAM read returned: " << (result == TPM_SUCCESS ? "ok" : "FAIL");

  if (result != TPM_SUCCESS) {
    status_ = result == TPM_E_BADINDEX ? Status::kAbsent : Status::kTpmError;
    return RESULT_FAIL_FATAL;
  }

  // Ignore defined but unwritten NVRAM area.
  uint8_t bytes_ored = 0x0;
  uint8_t bytes_anded = 0xff;
  for (uint8_t byte : buffer) {
    bytes_ored |= byte;
    bytes_anded &= byte;
  }
  if (bytes_ored == 0x0 || bytes_anded == 0xff) {
    status_ = Status::kAbsent;
    LOG(INFO) << "NVRAM area has been defined but not written.";
    return RESULT_FAIL_FATAL;
  }

  contents_.swap(buffer);
  status_ = Status::kValid;
  return RESULT_SUCCESS;
}

result_code NvramSpace::Write(const brillo::SecureBlob& contents) {
  VLOG(1) << "Writing NVRAM area " << index_ << " (size " << contents.size()
          << ")";

  if (!tpm_->available()) {
    return RESULT_FAIL_FATAL;
  }

  brillo::SecureBlob buffer(contents.size());
  uint32_t result = TlclWrite(index_, contents.data(), contents.size());

  VLOG(1) << "NVRAM write returned: "
          << (result == TPM_SUCCESS ? "ok" : "FAIL");

  if (result != TPM_SUCCESS) {
    return RESULT_FAIL_FATAL;
  }

  contents_ = contents;
  status_ = Status::kValid;
  return RESULT_SUCCESS;
}

result_code NvramSpace::ReadLock() {
  if (!tpm_->available()) {
    return RESULT_FAIL_FATAL;
  }

  uint32_t result = TlclReadLock(index_);
  if (result != TPM_SUCCESS) {
    LOG(ERROR) << "Failed to set read lock on NVRAM space " << index_;
    return RESULT_FAIL_FATAL;
  }

  return RESULT_SUCCESS;
}

result_code NvramSpace::WriteLock() {
  if (!tpm_->available()) {
    return RESULT_FAIL_FATAL;
  }

  uint32_t result = TlclWriteLock(index_);
  if (result != TPM_SUCCESS) {
    LOG(ERROR) << "Failed to set write lock on NVRAM space " << index_;
    return RESULT_FAIL_FATAL;
  }

  return RESULT_SUCCESS;
}

result_code NvramSpace::Define(uint32_t attributes, uint32_t size) {
  if (!tpm_->available()) {
    return RESULT_FAIL_FATAL;
  }

  uint32_t result = TlclDefineSpace(index_, attributes, size);
  if (result != TPM_SUCCESS) {
    LOG(ERROR) << "Failed to define NVRAM space " << index_ << ": " << result;
    return RESULT_FAIL_FATAL;
  }

  status_ = Status::kAbsent;
  contents_.clear();
  attributes_ = attributes;

  return RESULT_SUCCESS;
}

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

NvramSpace* Tpm::GetLockboxSpace() {
  if (lockbox_space_) {
    return lockbox_space_.get();
  }

  lockbox_space_.reset(new NvramSpace(this, kLockboxIndex));

  // Reading the NVRAM takes 40ms. Instead of querying the NVRAM area for its
  // size (which takes time), just read the expected size. If it fails, then
  // fall back to the older size. This means cleared devices take 80ms (2 failed
  // reads), legacy devices take 80ms (1 failed read, 1 good read), and
  // populated devices take 40ms, which is the minimum possible time (instead of
  // 40ms + time to query NVRAM size).
  if (lockbox_space_->Read(kLockboxSizeV2) == RESULT_SUCCESS) {
    LOG(INFO) << "Version 2 Lockbox NVRAM area found.";
  } else if (lockbox_space_->Read(kLockboxSizeV1) == RESULT_SUCCESS) {
    LOG(INFO) << "Version 1 Lockbox NVRAM area found.";
  } else {
    LOG(INFO) << "No Lockbox NVRAM area defined.";
  }

  if (lockbox_space_->is_valid()) {
    VLOG(1) << "lockbox nvram "
            << base::HexEncode(lockbox_space_->contents().data(),
                               lockbox_space_->contents().size());
  }

  return lockbox_space_.get();
}

NvramSpace* Tpm::GetEncStatefulSpace() {
  if (encstateful_space_) {
    return encstateful_space_.get();
  }

  encstateful_space_.reset(new NvramSpace(this, kEncStatefulIndex));

  if (encstateful_space_->Read(kEncStatefulSize) == RESULT_SUCCESS) {
    LOG(INFO) << "Found encstateful NVRAM area.";
  } else {
    LOG(INFO) << "No encstateful NVRAM area defined.";
  }

  return encstateful_space_.get();
}
