// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This macro must be defined before stdint.h is included explicitly or
// implicitly.  It ensures UINT32_MAX is available.
#define __STDC_LIMIT_MACROS
#include "lockbox.h"

#include <arpa/inet.h>
#include <openssl/sha.h>
#include <limits.h>
#include <stdint.h>

#include <base/logging.h>
#include <base/threading/platform_thread.h>
#include <base/time.h>
#include <chromeos/utility.h>

namespace cryptohome {
const uint32_t Lockbox::kNvramVersion1 = 1;
const uint32_t Lockbox::kNvramVersion2 = 2;
const uint32_t Lockbox::kNvramVersionDefault = 1;
const uint32_t Lockbox::kReservedSizeBytes = sizeof(uint32_t);
const uint32_t Lockbox::kReservedFlagsBytes = sizeof(uint8_t);
const uint32_t Lockbox::kReservedSaltBytesV1 = 7;
const uint32_t Lockbox::kReservedSaltBytesV2 = CRYPTOHOME_LOCKBOX_SALT_LENGTH;
const uint32_t Lockbox::kReservedDigestBytes = SHA256_DIGEST_LENGTH;
const uint32_t Lockbox::kReservedNvramBytesV1 = kReservedSizeBytes +
                                                kReservedFlagsBytes +
                                                kReservedSaltBytesV1 +
                                                kReservedDigestBytes;
const uint32_t Lockbox::kReservedNvramBytesV2 = kReservedSizeBytes +
                                                kReservedFlagsBytes +
                                                kReservedSaltBytesV2 +
                                                kReservedDigestBytes;


Lockbox::Lockbox(Tpm* tpm, uint32_t nvram_index)
  : tpm_(tpm),
    nvram_index_(nvram_index),
    nvram_version_(kNvramVersionDefault),
    default_crypto_(new Crypto()),
    crypto_(default_crypto_.get()),
    contents_(new LockboxContents()) {
}

Lockbox::~Lockbox() {
}

bool Lockbox::TpmIsReady() const {
  return tpm_ && tpm_->IsConnected() && tpm_->IsOwned();
}

bool Lockbox::HasAuthorization() const {
  if (!TpmIsReady())
    return false;
  // If we have a TPM owner password, we can reset.
  chromeos::Blob owner_password;
  return tpm_->GetOwnerPassword(&owner_password) && owner_password.size() != 0;
}

bool Lockbox::Destroy(ErrorId* error) {
  CHECK(error);
  *error = kErrorIdNone;
  if (!HasAuthorization())
    return false;
  // This is only an error if authorization is supplied.
  if (tpm_->IsNvramDefined(nvram_index_) &&
      !tpm_->DestroyNvram(nvram_index_)) {
    *error = kErrorIdTpmError;
    return false;
  }
  return true;
}

bool Lockbox::Create(ErrorId* error) {
  uint32_t nvram_bytes;
  CHECK(error);
  *error = kErrorIdNone;
  // Make sure we have what we need now.
  if (!HasAuthorization()) {
    *error = kErrorIdInsufficientAuthorization;
    LOG(ERROR) << "Create() called with insufficient authorization.";
    return false;
  }
  if (!Destroy(error)) {
    LOG(ERROR) << "Failed to destroy lockbox data before creation.";
    return false;
  }
  switch (nvram_version_) {
  case kNvramVersion1:
    nvram_bytes = kReservedNvramBytesV1;
    break;
  case kNvramVersion2:
  default:
    nvram_bytes = kReservedNvramBytesV2;
    break;
  }
  if (!tpm_->DefineLockOnceNvram(nvram_index_, nvram_bytes, 0)) {
    *error = kErrorIdTpmError;
    LOG(ERROR) << "Create() failed to defined NVRAM space.";
    return false;
  }
  LOG(INFO) << "Lockbox created.";
  return true;
}

bool Lockbox::Load(ErrorId* error) {
  CHECK(error);

  // TODO(wad) Determine if we want to allow reloading later.
  if (contents_->loaded)
    return true;

  if (!TpmIsReady()) {
    LOG(ERROR) << "Load() TPM is not ready.";
    *error = kErrorIdTpmNotReady;
    return false;
  }

  if (!tpm_->IsNvramDefined(nvram_index_)) {
    LOG(INFO) << "Load() called with no NVRAM space defined.";
    *error = kErrorIdNoNvramSpace;
    return false;
  }

  if (!tpm_->IsNvramLocked(nvram_index_)) {
    LOG(INFO) << "Load() called prior to a successful Store()";
    *error = kErrorIdNoNvramData;
    return false;
  }

  SecureBlob nvram_data(0);
  if (!tpm_->ReadNvram(nvram_index_, &nvram_data)) {
    LOG(ERROR) << "Load() could not read from NVRAM space.";
    *error = kErrorIdTpmError;
    return false;
  }

  // If the read is successful, but the size is not an expected value,
  // we've got tampering or an unexpected bug/race during set.
  switch (nvram_data.size()) {
  case kReservedNvramBytesV1:
    contents_->salt_size = kReservedSaltBytesV1;
    break;
  case kReservedNvramBytesV2:
    contents_->salt_size = kReservedSaltBytesV2;
    break;
  default:
    LOG(ERROR) << "Load() unexpected NVRAM size.";
    *error = kErrorIdNvramInvalid;
    return false;
  }

  // Extract the expected data size from the NVRAM.
  if (!ParseSizeBlob(nvram_data, &contents_->size)) {
    LOG(ERROR) << "Load() unable to parse NVRAM data.";
    *error = kErrorIdNvramInvalid;
    return false;
  }

  // Drop the size bytes
  nvram_data.erase(nvram_data.begin(),
                   nvram_data.begin() + kReservedSizeBytes);

  contents_->flags = nvram_data.front();
  // Erase the reserved flags byte(s).
  nvram_data.erase(nvram_data.begin(),
                   nvram_data.begin() + kReservedFlagsBytes);

  // Grab the salt.
  DCHECK(sizeof(contents_->salt) == kReservedSaltBytesV2);
  DCHECK(sizeof(contents_->salt) >= contents_->salt_size);
  memcpy(contents_->salt, &nvram_data[0], contents_->salt_size);
  nvram_data.erase(nvram_data.begin(),
                   nvram_data.begin() + contents_->salt_size);

  // Grab the hash.
  DCHECK(nvram_data.size() == kReservedDigestBytes);
  DCHECK(sizeof(contents_->hash) == kReservedDigestBytes);
  memcpy(contents_->hash, &nvram_data[0], sizeof(contents_->hash));

  DLOG(INFO) << "Load() successfully loaded NVRAM data.";
  contents_->loaded = true;
  return true;
}

bool Lockbox::Verify(const chromeos::Blob& blob, ErrorId* error) {
  CHECK(error);
  // It's not possible to verify without a locked space.
  if (!contents_->loaded) {
    *error = kErrorIdNoNvramData;
    return false;
  }

  // Make sure that the file size matches what was stored in nvram.
  if (blob.size() != contents_->size) {
    LOG(ERROR) << "Verify() expected " << contents_->size
               << " , but read " << blob.size() << " bytes.";
    *error = kErrorIdSizeMismatch;
    return false;
  }

  // Append the salt to the data.
  chromeos::Blob salty_blob(blob);
  salty_blob.insert(salty_blob.end(),
                    contents_->salt,
                    contents_->salt + contents_->salt_size);

  SecureBlob hash(0);
  crypto_->GetSha256(salty_blob, 0, salty_blob.size(), &hash);
  // Maybe release the duplicate blob data.
  // TODO(wad) Add Crypto::GatherSha256 which takes a vector of blobs.
  salty_blob.resize(0);

  DCHECK(hash.size() == kReservedDigestBytes);
  // Validate the data hash versus the stored hash.
  if (chromeos::SafeMemcmp(contents_->hash, hash.data(),
                           sizeof(contents_->hash))) {
    LOG(ERROR) << "Verify() hash mismatch!";
    *error = kErrorIdHashMismatch;
    return false;
  }
  DLOG(INFO) << "Verify() verified "
             << blob.size() << " of " << contents_->size << " bytes.";
  return true;
}

bool Lockbox::Store(const chromeos::Blob& blob, ErrorId* error) {
  if (!TpmIsReady()) {
    LOG(ERROR) << "Store() called when TPM was not ready!";
    *error = kErrorIdTpmError;
    return false;
  }

  // Grab a salt from the TPM.
  chromeos::Blob salt(0);
  switch (nvram_version_) {
  case kNvramVersion1:
    contents_->salt_size = kReservedSaltBytesV1;
    break;
  case kNvramVersion2:
  default:
    contents_->salt_size = kReservedSaltBytesV2;
    break;
  }
  if (!tpm_->GetRandomData(contents_->salt_size, &salt)) {
    LOG(ERROR) << "Store() failed to get a salt from the TPM.";
    *error = kErrorIdTpmError;
    return false;
  }
  // Keep the data locally too.
  DCHECK(sizeof(contents_->salt) == kReservedSaltBytesV2);
  DCHECK(sizeof(contents_->salt) >= contents_->salt_size);
  memcpy(contents_->salt, &salt[0], contents_->salt_size);

  // Get the size of the data blob
  chromeos::Blob size_blob;
  if (!GetSizeBlob(blob, &size_blob)) {
    LOG(ERROR) << "Store() data blob is too large.";
    *error = kErrorIdTooLarge;
    return false;
  }
  contents_->size = blob.size();

  // Append the salt to the data and hash.
  chromeos::Blob salty_blob(blob);
  salty_blob.insert(salty_blob.end(), salt.begin(), salt.end());

  SecureBlob nvram_blob(0);

  // Insert the hash into the NVRAM.
  crypto_->GetSha256(salty_blob, 0, salty_blob.size(), &nvram_blob);
  DCHECK(kReservedDigestBytes == nvram_blob.size());
  memcpy(contents_->hash, &nvram_blob[0], sizeof(contents_->hash));

  // Insert the salt into the NVRAM.
  nvram_blob.insert(nvram_blob.begin(), salt.begin(), salt.end());

  // Insert the flags byte. At present, this is always 0. It exists
  // to allow for future format changes that can be indicated without relying
  // on untrusted-file parsing-based detection, such as digest algorithm
  // changes or the addition of encryption.
  nvram_blob.insert(nvram_blob.begin(), static_cast<unsigned char>(0));
  contents_->flags = 0;

  // Insert size prefix.
  nvram_blob.insert(nvram_blob.begin(), size_blob.begin(), size_blob.end());

  // The resulting NVRAM space should look like:
  //   [size_blob][flags][salt][hash_blob]

  // Ensure we have the spaces ready.
  if (!tpm_->IsNvramDefined(nvram_index_)) {
    LOG(ERROR) << "Store() called with no NVRAM space.";
    *error = kErrorIdNoNvramSpace;
    return false;
  }
  if (tpm_->IsNvramLocked(nvram_index_)) {
    LOG(ERROR) << "Store() called with a locked NVRAM space.";
    *error = kErrorIdNvramInvalid;
    return false;
  }
  // Write the hash to nvram
  if (!tpm_->WriteNvram(nvram_index_, nvram_blob)) {
    LOG(ERROR) << "Store() failed to write the attribute hash to NVRAM";
    *error = kErrorIdTpmError;
    return false;
  }
  SecureBlob lock(0);
  // Write 0 to the nvram
  if (!tpm_->WriteNvram(nvram_index_, lock)) {
    LOG(ERROR) << "Store() failed to lock the NVRAM space";
    *error = kErrorIdTpmError;
    return false;
  }
  // Ensure the space is now locked.
  if (!tpm_->IsNvramLocked(nvram_index_)) {
    LOG(ERROR) << "NVRAM space did not lock as expected.";
    *error = kErrorIdNvramFailedToLock;
    return false;
  }
  return true;

  LOG(INFO) << "Store()'d " << blob.size() << " bytes";
  return true;
}

bool Lockbox::GetSizeBlob(const chromeos::Blob& data,
                          chromeos::Blob* size_bytes) const {
  uint32_t serializable_size = 0;
  if (data.size() > UINT32_MAX)
    return false;
  // Use network byte order prior to marshalling to ensure safe
  // unmarshalling if we somehow change endianness.
  serializable_size = htonl(static_cast<uint32_t>(data.size()));
  // Push it back starting with the first byte.
  size_bytes->resize(0);
  for (uint32_t bytes = 0; bytes < kReservedSizeBytes; ++bytes) {
    size_bytes->push_back(
      static_cast<char>((serializable_size >> (CHAR_BIT * bytes)) & 0xff));
  }
  return true;
}

bool Lockbox::ParseSizeBlob(const chromeos::Blob& blob, uint32_t* size) const {
  CHECK(size);
  *size = 0;
  if (blob.size() < kReservedSizeBytes)
    return false;
  // Unmarshal it from NVRAM based on how it was written.
  uint32_t stored_size = 0;
  for (uint32_t bytes = 0; bytes < kReservedSizeBytes; ++bytes) {
    stored_size |= static_cast<uint32_t>(blob.at(bytes)) << (bytes * CHAR_BIT);
  }
  // Now convert back from network byte order.
  *size = ntohl(stored_size);
  return true;
}
}  // namespace cryptohome
