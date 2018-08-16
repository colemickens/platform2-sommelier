// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/mount_encrypted/tpm.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <memory>

#include <base/logging.h>
#include <base/macros.h>
#include <base/strings/string_number_conversions.h>

#include <vboot/tlcl.h>

#include "cryptohome/cryptolib.h"
#include "cryptohome/mount_encrypted/mount_encrypted.h"

namespace {

// TPM2 NVRAM area and related constants.
const uint32_t kNvramAreaTpm2Index = 0x800005;
const uint32_t kNvramAreaTpm2Magic = 0x54504D32;
const uint32_t kNvramAreaTpm2VersionMask = 0x000000FF;
const uint32_t kNvramAreaTpm2CurrentVersion = 1;

const uint32_t kAttributes = TPMA_NV_AUTHWRITE | TPMA_NV_AUTHREAD |
                             TPMA_NV_WRITEDEFINE | TPMA_NV_READ_STCLEAR;

struct nvram_area_tpm2 {
  uint32_t magic;
  uint32_t ver_flags;
  uint8_t key_material[DIGEST_LENGTH];
};

bool IsSpacePresent(NvramSpace* space) {
  uint32_t attributes = 0;
  return space->GetAttributes(&attributes) == RESULT_SUCCESS;
}

// Derive the system key from the key material in |area|.
brillo::SecureBlob DeriveSystemKey(const struct nvram_area_tpm2* area) {
  brillo::SecureBlob system_key =
      cryptohome::CryptoLib::Sha256(brillo::SecureBlob(
          area->key_material, area->key_material + sizeof(area->key_material)));

  VLOG(1) << "system key "
          << base::HexEncode(system_key.data(), system_key.size());

  return system_key;
}

}  // namespace

const uint8_t* kOwnerSecret = nullptr;
const size_t kOwnerSecretSize = 0;

class Tpm2SystemKeyLoader : public SystemKeyLoader {
 public:
  explicit Tpm2SystemKeyLoader(Tpm* tpm) : tpm_(tpm) {}

  result_code Load(brillo::SecureBlob* key) override;
  brillo::SecureBlob Generate() override;
  result_code Persist() override;
  void Lock() override;
  result_code SetupTpm() override;
  result_code GenerateForPreservation(brillo::SecureBlob* previous_key,
                                      brillo::SecureBlob* fresh_key) override;
  result_code CheckLockbox(bool* valid) override;
  bool UsingLockboxKey() override;

 private:
  Tpm* tpm_ = nullptr;

  // Provisional space contents that get initialized by Generate() and written
  // to the NVRAM space by Persist();
  std::unique_ptr<brillo::SecureBlob> provisional_contents_;

  DISALLOW_COPY_AND_ASSIGN(Tpm2SystemKeyLoader);
};

// For TPM2, NVRAM area is separate from Lockbox.
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
//  - *system_key populated with NVRAM area entropy.
// In case of failure: (NVRAM missing or error)
//  - *system_key untouched.
result_code Tpm2SystemKeyLoader::Load(brillo::SecureBlob* system_key) {
  LOG(INFO) << "Getting key from TPM2 NVRAM index " << kNvramAreaTpm2Index;

  if (!tpm_->available()) {
    return RESULT_FAIL_FATAL;
  }

  NvramSpace* encstateful_space = tpm_->GetEncStatefulSpace();
  if (!IsSpacePresent(encstateful_space) || !encstateful_space->is_valid() ||
      encstateful_space->contents().size() < sizeof(struct nvram_area_tpm2)) {
    LOG(INFO) << "NVRAM area doesn't exist or is invalid";
    return RESULT_FAIL_FATAL;
  }

  const struct nvram_area_tpm2* area =
      reinterpret_cast<const struct nvram_area_tpm2*>(
          encstateful_space->contents().data());
  if (area->magic != kNvramAreaTpm2Magic ||
      (area->ver_flags & kNvramAreaTpm2VersionMask) !=
          kNvramAreaTpm2CurrentVersion) {
    return RESULT_FAIL_FATAL;
  }

  *system_key = DeriveSystemKey(area);
  return RESULT_SUCCESS;
  }

brillo::SecureBlob Tpm2SystemKeyLoader::Generate() {
  provisional_contents_ =
      std::make_unique<brillo::SecureBlob>(sizeof(nvram_area_tpm2));
  struct nvram_area_tpm2* area =
      reinterpret_cast<struct nvram_area_tpm2*>(provisional_contents_->data());
  area->magic = kNvramAreaTpm2Magic;
  area->ver_flags = kNvramAreaTpm2CurrentVersion;
  cryptohome::CryptoLib::GetSecureRandom(area->key_material,
                                         sizeof(area->key_material));

  VLOG(1) << "key nvram "
          << base::HexEncode(provisional_contents_->data(),
                             provisional_contents_->size());

  return DeriveSystemKey(area);
}

result_code Tpm2SystemKeyLoader::Persist() {
  CHECK(provisional_contents_);

  NvramSpace* encstateful_space = tpm_->GetEncStatefulSpace();
  if (!IsSpacePresent(encstateful_space)) {
    result_code rc = encstateful_space->Define(
        kAttributes, sizeof(struct nvram_area_tpm2), 0);
    if (rc != RESULT_SUCCESS) {
      LOG(ERROR) << "Failed to define NVRAM space.";
      return rc;
    }
  }

  result_code rc = encstateful_space->Write(*provisional_contents_);
  if (rc != RESULT_SUCCESS) {
    uint32_t attributes = 0;
    encstateful_space->GetAttributes(&attributes);
    LOG(ERROR) << "Failed to write NVRAM area. Attributes: " << attributes;
    return rc;
  }

  return RESULT_SUCCESS;
}

void Tpm2SystemKeyLoader::Lock() {
  // Lock the area as needed. Write-lock may be already set.
  // Read-lock is never set at this point, since we were able to read.
  // Not being able to lock is not fatal, though exposes the key.
  uint32_t attributes = 0;
  NvramSpace* encstateful_space = tpm_->GetEncStatefulSpace();
  if (encstateful_space->GetAttributes(&attributes) != RESULT_SUCCESS) {
    LOG(ERROR) << "Failed to read attributes";
    return;
  }

  if (!(attributes & TPMA_NV_WRITELOCKED) &&
      encstateful_space->WriteLock() != RESULT_SUCCESS) {
    LOG(ERROR) << "Failed to write-lock NVRAM area.";
  }
  if (encstateful_space->ReadLock() != RESULT_SUCCESS) {
    LOG(ERROR) << "Failed to read-lock NVRAM area.";
  }
}

result_code Tpm2SystemKeyLoader::SetupTpm() {
  // NVRAM indexes can be defined without requiring special privileges, so
  // there's nothing to do here.
  return RESULT_SUCCESS;
}

result_code Tpm2SystemKeyLoader::GenerateForPreservation(
    brillo::SecureBlob* previous_key, brillo::SecureBlob* fresh_key) {
  LOG(FATAL) << "Preservation not implemented for TPM 2.0";
  return RESULT_FAIL_FATAL;
}

result_code Tpm2SystemKeyLoader::CheckLockbox(bool* valid) {
  // Lockbox is valid only once the TPM is owned.
  return tpm_->IsOwned(valid);
}

bool Tpm2SystemKeyLoader::UsingLockboxKey() {
  // TPM 2 systems never fall back to using the lockbox salt.
  return false;
}

std::unique_ptr<SystemKeyLoader> SystemKeyLoader::Create(
    Tpm* tpm, const base::FilePath& rootdir) {
  return std::make_unique<Tpm2SystemKeyLoader>(tpm);
}
