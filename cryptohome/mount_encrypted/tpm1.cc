// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/mount_encrypted/tpm.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <type_traits>

#include <base/logging.h>
#include <base/macros.h>
#include <base/strings/string_number_conversions.h>

#include "cryptohome/cryptolib.h"
#include "cryptohome/mount_encrypted.h"

namespace {

const uint32_t kLockboxSaltOffset = 0x5;

// Attributes for the encstatful NVRAM space. Ideally, we'd set
// TPM_NV_PER_OWNERWRITE so the space gets automatically destroyed when the TPM
// gets cleared. That'd mean we'd have to recreate the NVRAM space on next boot
// though, which requires TPM ownership. Taking ownership is notoriously slow,
// so we can't afford to do this. Instead, we keep the space allocated and
// detect TPM clear to regenerate the system key.
const uint32_t kAttributes = TPM_NV_PER_WRITE_STCLEAR | TPM_NV_PER_READ_STCLEAR;

const uint32_t kAttributesMask =
    TPM_NV_PER_READ_STCLEAR | TPM_NV_PER_AUTHREAD | TPM_NV_PER_OWNERREAD |
    TPM_NV_PER_PPREAD | TPM_NV_PER_GLOBALLOCK | TPM_NV_PER_WRITE_STCLEAR |
    TPM_NV_PER_WRITEDEFINE | TPM_NV_PER_WRITEALL | TPM_NV_PER_AUTHWRITE |
    TPM_NV_PER_OWNERWRITE | TPM_NV_PER_PPWRITE;

// Key derivation labels.
const char kLabelSystemKey[] = "system_key";

// This struct defines the memory layout of the NVRAM area. Member sizes are
// chosen taking layout into consideration.
struct EncStatefulArea {
  enum class Flag {
  };

  static constexpr uint32_t kMagic = 0x54504d31;

  static constexpr size_t kVersionShift = 8;
  static constexpr uint32_t kVersionMask = (1 << kVersionShift) - 1;

  static constexpr uint32_t kCurrentVersion = 1;

  uint32_t magic;
  uint32_t ver_flags;
  uint8_t key_material[DIGEST_LENGTH];
  uint8_t lockbox_mac[DIGEST_LENGTH];

  bool is_valid() const {
    return magic == kMagic && (ver_flags & kVersionMask) == kCurrentVersion;
  }

  static uint32_t flag_value(Flag flag) {
    return (1 << (static_cast<size_t>(flag) + kVersionShift));
  }

  bool test_flag(Flag flag) const {
    return ver_flags & flag_value(flag);
  }
  void set_flag(Flag flag) {
    ver_flags |= flag_value(flag);
  }
  void clear_flag(Flag flag) {
    ver_flags &= ~flag_value(flag);
  }

  void Init() {
    magic = kMagic;
    ver_flags = kCurrentVersion;
    cryptohome::CryptoLib::GetSecureRandom(key_material, sizeof(key_material));
    memset(lockbox_mac, 0, sizeof(lockbox_mac));
  }

  brillo::SecureBlob DeriveKey(const std::string& label) const {
    return cryptohome::CryptoLib::HmacSha256(
        brillo::SecureBlob(key_material, key_material + sizeof(key_material)),
        brillo::Blob(label.data(), label.data() + label.size()));
  }
};

// We're using casts to map the EncStatefulArea struct to NVRAM contents, so
// make sure to keep this a POD type.
static_assert(std::is_pod<EncStatefulArea>(),
              "EncStatefulArea must be a POD type");

// Make sure that the EncStatefulArea struct fits the encstateful NVRAM space.
static_assert(kEncStatefulSize >= sizeof(EncStatefulArea),
              "EncStatefulArea must fit within encstateful NVRAM space");

}  // namespace

// System key loader implementation for TPM1 systems. This supports two
// different sources of obtaining system key material: A dedicated NVRAM space
// (called the "encstateful NVRAM space" below) and the "salt" in the lockbox
// space. We prefer the former if it is available.
class Tpm1SystemKeyLoader : public SystemKeyLoader {
 public:
  explicit Tpm1SystemKeyLoader(Tpm* tpm) : tpm_(tpm) {}

  result_code Load(brillo::SecureBlob* key, bool* migrate) override;
  brillo::SecureBlob Generate() override;
  result_code Persist() override;
  void Lock() override;

 private:
  // Loads the key from the encstateful NVRAM space.
  result_code LoadEncStatefulKey(brillo::SecureBlob* key);

  // Loads the key from the lockbox NVRAM space.
  result_code LoadLockboxKey(brillo::SecureBlob* key, bool* migrate);

  // Validates the encstateful space is defined with correct parameters.
  result_code IsEncStatefulSpaceProperlyDefined(bool* result);

  Tpm* tpm_ = nullptr;

  // Provisional space contents that get initialized by Generate() and written
  // to the NVRAM space by Persist();
  std::unique_ptr<brillo::SecureBlob> provisional_contents_;

  DISALLOW_COPY_AND_ASSIGN(Tpm1SystemKeyLoader);
};

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
result_code Tpm1SystemKeyLoader::Load(brillo::SecureBlob* system_key,
                                      bool* migrate) {
  *migrate = false;

  bool space_properly_defined = false;
  result_code rc = IsEncStatefulSpaceProperlyDefined(&space_properly_defined);
  if (rc != RESULT_SUCCESS) {
    return rc;
  }

  // Prefer the encstateful space if it is set up correctly.
  if (space_properly_defined) {
    // Only load the key if we are sure that we have generated a fresh key after
    // the last TPM clear. After a clear, the TPM has no owner. In unowned state
    // we rely on a flag we store persistently in the TPM to indicate whether we
    // have generated a key already (note that the TPM automatically clears the
    // flag on TPM clear).
    bool system_key_initialized = false;
    rc = tpm_->HasSystemKeyInitializedFlag(&system_key_initialized);
    if (rc != RESULT_SUCCESS) {
      return rc;
    }

    if (system_key_initialized) {
      rc = LoadEncStatefulKey(system_key);
      if (rc == RESULT_SUCCESS) {
        return RESULT_SUCCESS;
      }
    }
  } else {
    // The lockbox NVRAM space is created by cryptohomed and only valid after
    // TPM ownership has been established.
    bool owned = false;
    result_code rc = tpm_->IsOwned(&owned);
    if (rc != RESULT_SUCCESS) {
      LOG(ERROR) << "Failed to determine TPM ownership.";
      return rc;
    }

    if (owned) {
      rc = LoadLockboxKey(system_key, migrate);
      if (rc == RESULT_SUCCESS) {
        return RESULT_SUCCESS;
      }
    }
  }

  // If there's no key yet, allow migration.
  *migrate = true;

  return RESULT_FAIL_FATAL;
}

brillo::SecureBlob Tpm1SystemKeyLoader::Generate() {
  provisional_contents_ =
      std::make_unique<brillo::SecureBlob>(sizeof(EncStatefulArea));
  EncStatefulArea* area =
      reinterpret_cast<EncStatefulArea*>(provisional_contents_->data());
  area->Init();
  return area->DeriveKey(kLabelSystemKey);
}

result_code Tpm1SystemKeyLoader::Persist() {
  CHECK(provisional_contents_);

  bool space_defined_properly = false;
  result_code rc = IsEncStatefulSpaceProperlyDefined(&space_defined_properly);
  if (rc != RESULT_SUCCESS) {
    return rc;
  }

  if (!space_defined_properly) {
    return RESULT_FAIL_FATAL;
  }

  NvramSpace* encstateful_space = tpm_->GetEncStatefulSpace();
  rc = encstateful_space->Write(*provisional_contents_);
  if (rc != RESULT_SUCCESS) {
    LOG(ERROR) << "Failed to write NVRAM area";
    return rc;
  }

  rc = tpm_->SetSystemKeyInitializedFlag();
  if (rc != RESULT_SUCCESS) {
    LOG(ERROR) << "Failed to create dummy delegation entry.";
    return rc;
  }

  return RESULT_SUCCESS;
}

void Tpm1SystemKeyLoader::Lock() {
  NvramSpace* encstateful_space = tpm_->GetEncStatefulSpace();
  if (!encstateful_space->is_valid()) {
    return;
  }

  if (!encstateful_space->WriteLock()) {
    LOG(ERROR) << "Failed to write-lock NVRAM area.";
  }
  if (!encstateful_space->ReadLock()) {
    LOG(ERROR) << "Failed to read-lock NVRAM area.";
  }
}

result_code Tpm1SystemKeyLoader::LoadEncStatefulKey(
    brillo::SecureBlob* system_key) {
  const EncStatefulArea* area = reinterpret_cast<const EncStatefulArea*>(
      tpm_->GetEncStatefulSpace()->contents().data());
  if (!area->is_valid()) {
    LOG(ERROR) << "Invalid encstateful contents.";
    return RESULT_FAIL_FATAL;
  }

  VLOG(1) << "key material "
          << base::HexEncode(area->key_material, sizeof(area->key_material));
  *system_key = area->DeriveKey(kLabelSystemKey);
  VLOG(1) << "system_key "
          << base::HexEncode(system_key->data(), system_key->size());

  return RESULT_SUCCESS;
}

result_code Tpm1SystemKeyLoader::LoadLockboxKey(brillo::SecureBlob* system_key,
                                                bool* migrate) {
  brillo::SecureBlob key_material;
  NvramSpace* lockbox_space = tpm_->GetLockboxSpace();
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

result_code Tpm1SystemKeyLoader::IsEncStatefulSpaceProperlyDefined(
    bool* result) {
  *result = false;

  NvramSpace* encstateful_space = tpm_->GetEncStatefulSpace();
  if (!encstateful_space->is_valid() ||
      encstateful_space->contents().size() < sizeof(EncStatefulArea)) {
    LOG(ERROR) << "encstateful space contents absent or too short.";
    return RESULT_SUCCESS;
  }

  uint32_t attributes;
  result_code rc = encstateful_space->GetAttributes(&attributes);
  if (rc != RESULT_SUCCESS) {
    return rc;
  }

  if ((attributes & kAttributesMask) != kAttributes) {
    LOG(ERROR) << "Bad encstateful space attributes.";
    return rc;
  }

  uint32_t pcr_selection = (1 << kPCRBootMode);
  bool pcr_binding_correct = false;
  rc = encstateful_space->CheckPCRBinding(pcr_selection, &pcr_binding_correct);
  if (rc != RESULT_SUCCESS) {
    LOG(ERROR) << "Bad encstateful PCR binding.";
    return rc;
  }

  *result = pcr_binding_correct;
  return RESULT_SUCCESS;
}

std::unique_ptr<SystemKeyLoader> SystemKeyLoader::Create(Tpm* tpm) {
  return std::make_unique<Tpm1SystemKeyLoader>(tpm);
}
