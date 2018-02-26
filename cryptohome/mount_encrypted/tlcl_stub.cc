// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/mount_encrypted/tlcl_stub.h"

#include <algorithm>

#include <base/logging.h>

#include <openssl/sha.h>

#include <vboot/tlcl.h>

#include <brillo/secure_blob.h>

#include "cryptohome/cryptolib.h"

namespace {

#if !USE_TPM2

const size_t kMaxDelegationFamilyTableSize = 8;

#endif  // !USE_TPM2

}  // namespace

TlclStub* TlclStub::g_instance = nullptr;

TlclStub::TlclStub() {
  g_instance = this;
}

TlclStub::~TlclStub() {
  g_instance = nullptr;
}

TlclStub::NvramSpaceData* TlclStub::GetSpace(uint32_t index) {
  return &nvram_spaces_[index];
}

void TlclStub::SetOwned(const std::vector<uint8_t>& owner_auth) {
  owner_auth_ = owner_auth;
}

void TlclStub::SetPCRValue(uint32_t index,
                           const uint8_t value[TPM_PCR_DIGEST]) {
  memcpy(pcr_values_[index], value, TPM_PCR_DIGEST);
}

TlclStub* TlclStub::Get() {
  CHECK(g_instance);
  return g_instance;
}

uint32_t TlclStub::GetOwnership(uint8_t* owned) {
  *owned = is_owned();
  return TPM_SUCCESS;
}

uint32_t TlclStub::GetRandom(uint8_t* data, uint32_t length, uint32_t* size) {
  memset(data, '0x5a', length);
  *size = length;
  return TPM_SUCCESS;
}

uint32_t TlclStub::DefineSpace(uint32_t index, uint32_t perm, uint32_t size) {
  bool authenticated = false;

#if USE_TPM2
  // NVRAM space creation in normal mode only works as long as the TPM isn't
  // owned yet. Only non-existing spaces can be defined.
  authenticated = !is_owned() && nvram_spaces_.count(index) == 0;
#endif

  if (!authenticated) {
    return TPM_E_AUTHFAIL;
  }

  nvram_spaces_[index] = NvramSpaceData();
  nvram_spaces_[index].attributes = perm;
  nvram_spaces_[index].contents.resize(size);
  return TPM_SUCCESS;
}

uint32_t TlclStub::GetPermissions(uint32_t index, uint32_t* permissions) {
  return WithSpace(index, [=](NvramSpaceData* space) {
    *permissions = space->attributes;
    return TPM_SUCCESS;
  });
}

uint32_t TlclStub::GetSpaceInfo(uint32_t index,
                                uint32_t* permissions,
                                uint32_t* size,
                                void* auth_policy,
                                uint32_t* auth_policy_size) {
  return WithSpace(index, [=](NvramSpaceData* space) {
    if (space->policy.size() > *auth_policy_size) {
      *auth_policy_size = space->policy.size();
      return TPM_E_BUFFER_SIZE;
    }

    *permissions = space->attributes;
    *size = space->contents.size();
    memcpy(auth_policy, space->policy.data(), space->policy.size());
    *auth_policy_size = space->policy.size();
    return TPM_SUCCESS;
  });
}

uint32_t TlclStub::Write(uint32_t index, const void* data, uint32_t length) {
  return WithSpace(index, [=](NvramSpaceData* space) {
    if (length > space->contents.size()) {
      return TPM_E_INTERNAL_ERROR;  // should be TPM_NOSPACE
    }
    if (space->write_locked) {
      return TPM_E_INTERNAL_ERROR;  // should be TPM_AREA_LOCKED
    }
    memcpy(space->contents.data(), data, length);
#if USE_TPM2
    space->attributes |= TPMA_NV_WRITTEN;
#endif
    return TPM_SUCCESS;
  });
}

uint32_t TlclStub::Read(uint32_t index, void* data, uint32_t length) {
  return WithSpace(index, [=](NvramSpaceData* space) {
#if USE_TPM2
    if ((space->attributes & TPMA_NV_WRITTEN) != TPMA_NV_WRITTEN) {
      return TPM_E_BADINDEX;
    }
#endif
    if (length > space->contents.size()) {
      return TPM_E_INTERNAL_ERROR;  // should be TPM_NOSPACE
    }
    if (space->read_locked) {
      return TPM_E_INTERNAL_ERROR;  // should be TPM_AREA_LOCKED
    }
    memcpy(data, space->contents.data(),
           std::min(space->contents.size(), static_cast<size_t>(length)));
    return TPM_SUCCESS;
  });
}

uint32_t TlclStub::WriteLock(uint32_t index) {
  return WithSpace(index, [=](NvramSpaceData* space) {
    if (space->write_locked) {
      return TPM_E_INTERNAL_ERROR;  // should be TPM_AREA_LOCKED
    }
    space->write_locked = true;
    return TPM_SUCCESS;
  });
}

uint32_t TlclStub::ReadLock(uint32_t index) {
  return WithSpace(index, [=](NvramSpaceData* space) {
    if (space->read_locked) {
      return TPM_E_INTERNAL_ERROR;  // should be TPM_AREA_LOCKED
    }
    space->read_locked = true;
    return TPM_SUCCESS;
  });
}

uint32_t TlclStub::PCRRead(uint32_t index, void *data, uint32_t length) {
  if (length < TPM_PCR_DIGEST) {
    return TPM_E_BUFFER_SIZE;
  }

  auto entry = pcr_values_.find(index);
  if (entry != pcr_values_.end()) {
    memcpy(data, entry->second, TPM_PCR_DIGEST);
  } else {
    memset(data, 0, TPM_PCR_DIGEST);
  }

  return TPM_SUCCESS;
}

#if !USE_TPM2

uint32_t TlclStub::CreateDelegationFamily(uint8_t family_label) {
  if (is_owned()) {
    return TPM_E_OWNER_SET;
  }

  if (delegation_family_table_.size() >= kMaxDelegationFamilyTableSize) {
    return TPM_E_INTERNAL_ERROR;
  }

  delegation_family_table_.push_back(
      {0, family_label, ++delegation_family_id_, 1, 0});

  return TPM_SUCCESS;
}

uint32_t TlclStub::ReadDelegationFamilyTable(TPM_FAMILY_TABLE_ENTRY* table,
                                             uint32_t* table_size) {
  if (*table_size < delegation_family_table_.size()) {
    return TPM_E_BUFFER_SIZE;
  }

  *table_size = delegation_family_table_.size();
  std::copy(delegation_family_table_.begin(), delegation_family_table_.end(),
            table);

  return TPM_SUCCESS;
}

#endif  // !USE_TPM2

template<typename Action>
uint32_t TlclStub::WithSpace(uint32_t index, Action action) {
  auto entry = nvram_spaces_.find(index);
  if (entry == nvram_spaces_.end()) {
    return TPM_E_BADINDEX;
  }

  return action(&entry->second);
}

extern "C" {

uint32_t TlclLibInit(void) {
  // Check that a stub has been set up.
  CHECK(TlclStub::Get());
  return TPM_SUCCESS;
}

uint32_t TlclLibClose(void) {
  return TPM_SUCCESS;
}

uint32_t TlclGetOwnership(uint8_t* owned) {
  return TlclStub::Get()->GetOwnership(owned);
}

uint32_t TlclGetRandom(uint8_t* data, uint32_t length, uint32_t* size) {
  return TlclStub::Get()->GetRandom(data, length, size);
}

uint32_t TlclDefineSpace(uint32_t index, uint32_t perm, uint32_t size) {
  return TlclStub::Get()->DefineSpace(index, perm, size);
}

uint32_t TlclGetPermissions(uint32_t index, uint32_t *permissions) {
  return TlclStub::Get()->GetPermissions(index, permissions);
}

uint32_t TlclGetSpaceInfo(uint32_t index,
                          uint32_t* attributes,
                          uint32_t* size,
                          void* auth_policy,
                          uint32_t* auth_policy_size) {
  return TlclStub::Get()->GetSpaceInfo(index, attributes, size, auth_policy,
                                       auth_policy_size);
}

uint32_t TlclWrite(uint32_t index, const void *data, uint32_t length) {
  return TlclStub::Get()->Write(index, data, length);
}

uint32_t TlclRead(uint32_t index, void *data, uint32_t length) {
  return TlclStub::Get()->Read(index, data, length);
}

uint32_t TlclWriteLock(uint32_t index) {
  return TlclStub::Get()->WriteLock(index);
}

uint32_t TlclReadLock(uint32_t index) {
  return TlclStub::Get()->ReadLock(index);
}

uint32_t TlclPCRRead(uint32_t index, void *data, uint32_t length) {
  return TlclStub::Get()->PCRRead(index, data, length);
}

uint32_t TlclInitNvAuthPolicy(uint32_t pcr_selection_bitmap,
                              const uint8_t pcr_values[][TPM_PCR_DIGEST],
                              void* auth_policy,
                              uint32_t* auth_policy_size) {
  int buffer_size = *auth_policy_size;
  *auth_policy_size = SHA256_DIGEST_LENGTH;
  if (buffer_size < SHA256_DIGEST_LENGTH) {
    return TPM_E_BUFFER_SIZE;
  }

  std::vector<uint8_t> input(32);
  for (int index = 0; index < 32; ++index) {
    input[index] = (pcr_selection_bitmap & (1 << index)) != 0;
    if (input[index]) {
      input.insert(input.end(), *pcr_values, *pcr_values + TPM_PCR_DIGEST);
      ++pcr_values;
    }
  }

  brillo::SecureBlob digest = cryptohome::CryptoLib::Sha256(input);
  memcpy(auth_policy, digest.data(), digest.size());
  return TPM_SUCCESS;
}

#if !USE_TPM2

uint32_t TlclCreateDelegationFamily(uint8_t family_label) {
  return TlclStub::Get()->CreateDelegationFamily(family_label);
}

uint32_t TlclReadDelegationFamilyTable(TPM_FAMILY_TABLE_ENTRY *table,
                                       uint32_t* table_size) {
  return TlclStub::Get()->ReadDelegationFamilyTable(table, table_size);
}

#endif  // !USE_TPM2

}  // extern "C"
