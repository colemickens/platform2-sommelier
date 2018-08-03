// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/lockbox.h"

#include <arpa/inet.h>
#include <limits.h>
#include <openssl/sha.h>
#include <stdint.h>

#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/logging.h>
#include <base/strings/string_split.h>
#include <base/threading/platform_thread.h>
#include <base/time/time.h>
#include <brillo/secure_blob.h>

#include "cryptohome/cryptolib.h"
#include "cryptohome/platform.h"

using base::FilePath;
using brillo::SecureBlob;

namespace cryptohome {

std::ostream& operator<<(std::ostream& out, LockboxError error) {
  return out << static_cast<int>(error);
}

const uint32_t Lockbox::kNvramVersion1 = 1;
const uint32_t Lockbox::kNvramVersion2 = 2;
const uint32_t Lockbox::kNvramVersionDefault = 2;
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
const char * const Lockbox::kMountEncrypted = "/usr/sbin/mount-encrypted";
const char * const Lockbox::kMountEncryptedFinalize = "finalize";


Lockbox::Lockbox(Tpm* tpm, uint32_t nvram_index)
  : tpm_(tpm),
    nvram_index_(nvram_index),
    nvram_version_(kNvramVersionDefault),
    default_process_(new brillo::ProcessImpl()),
    process_(default_process_.get()),
    contents_(new LockboxContents()),
    default_platform_(new Platform()),
    platform_(default_platform_.get()) {
}

Lockbox::~Lockbox() {
}

bool Lockbox::TpmIsReady() const {
  if (!tpm_) {
    LOG(ERROR) << "TpmIsReady: no tpm_ instance.";
    return false;
  }
  if (!tpm_->IsEnabled()) {
    LOG(ERROR) << "TpmIsReady: is not enabled.";
    return false;
  }
  if (!tpm_->IsOwned()) {
    LOG(ERROR) << "TpmIsReady: is not owned.";
    return false;
  }
  return true;
}

bool Lockbox::HasAuthorization() const {
  if (!TpmIsReady()) {
    LOG(ERROR) << "HasAuthorization: TPM not ready.";
    return false;
  }
  // If we have a TPM owner password, we can reset.
  brillo::Blob owner_password;
  if (tpm_->GetOwnerPassword(&owner_password) && owner_password.size() != 0)
    return true;
  LOG(INFO) << "HasAuthorization: TPM Owner password not available.";
  return false;
}

bool Lockbox::Destroy(LockboxError* error) {
  CHECK(error);
  *error = LockboxError::kNone;
  if (!HasAuthorization())
    return false;
  // This is only an error if authorization is supplied.
  if (tpm_->IsNvramDefined(nvram_index_) &&
      !tpm_->DestroyNvram(nvram_index_)) {
    *error = LockboxError::kTpmError;
    return false;
  }
  return true;
}

bool Lockbox::Create(LockboxError* error) {
  uint32_t nvram_bytes;
  CHECK(error);
  *error = LockboxError::kNone;
  // Make sure we have what we need now.
  if (!HasAuthorization()) {
    *error = LockboxError::kInsufficientAuthorization;
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

  // If we store the encryption salt in lockbox, protect it from reading
  // in non-verified boot mode.
  uint32_t nvram_perm = Tpm::kTpmNvramWriteDefine |
    (IsEncryptionSaltInLockbox() ? Tpm::kTpmNvramBindToPCR0 : 0);
  if (!tpm_->DefineNvram(nvram_index_, nvram_bytes, nvram_perm)) {
    *error = LockboxError::kTpmError;
    LOG(ERROR) << "Create() failed to defined NVRAM space.";
    return false;
  }
  LOG(INFO) << "Lockbox created.";
  return true;
}

bool Lockbox::Load(LockboxError* error) {
  CHECK(error);

  // TODO(wad) Determine if we want to allow reloading later.
  if (contents_->loaded)
    return true;

  if (!TpmIsReady()) {
    LOG(ERROR) << "Load() TPM is not ready.";
    *error = LockboxError::kTpmNotReady;
    return false;
  }

  if (!tpm_->IsNvramDefined(nvram_index_)) {
    LOG(INFO) << "Load() called with no NVRAM space defined.";
    *error = LockboxError::kNoNvramSpace;
    return false;
  }

  if (!tpm_->IsNvramLocked(nvram_index_)) {
    LOG(INFO) << "Load() called prior to a successful Store()";
    *error = LockboxError::kNoNvramData;
    return false;
  }

  SecureBlob nvram_data(0);
  if (!tpm_->ReadNvram(nvram_index_, &nvram_data)) {
    LOG(ERROR) << "Load() could not read from NVRAM space.";
    *error = LockboxError::kTpmError;
    return false;
  }

  // If the read is successful, but the size is not an expected value,
  // we've got tampering or an unexpected bug/race during set.
  switch (nvram_data.size()) {
  case kReservedNvramBytesV1:
    contents_->salt_size = kReservedSaltBytesV1;
    nvram_version_ = kNvramVersion1;
    break;
  case kReservedNvramBytesV2:
    contents_->salt_size = kReservedSaltBytesV2;
    nvram_version_ = kNvramVersion2;
    break;
  default:
    LOG(ERROR) << "Load() found unexpected NVRAM size: " << nvram_data.size();
    *error = LockboxError::kNvramInvalid;
    return false;
  }

  // Extract the expected data size from the NVRAM.
  if (!ParseSizeBlob(nvram_data, &contents_->size)) {
    LOG(ERROR) << "Load() unable to parse NVRAM data.";
    *error = LockboxError::kNvramInvalid;
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
  memcpy(contents_->salt, nvram_data.data(), contents_->salt_size);
  nvram_data.erase(nvram_data.begin(),
                   nvram_data.begin() + contents_->salt_size);

  // Grab the hash.
  DCHECK(nvram_data.size() == kReservedDigestBytes);
  DCHECK(sizeof(contents_->hash) == kReservedDigestBytes);
  memcpy(contents_->hash, nvram_data.data(), sizeof(contents_->hash));

  DLOG(INFO) << "Load() successfully loaded NVRAM data.";
  contents_->loaded = true;
  return true;
}

bool Lockbox::Verify(const brillo::Blob& blob, LockboxError* error) {
  CHECK(error);
  // It's not possible to verify without a locked space.
  if (!contents_->loaded) {
    *error = LockboxError::kNoNvramData;
    return false;
  }

  // Make sure that the file size matches what was stored in nvram.
  if (blob.size() != contents_->size) {
    LOG(ERROR) << "Verify() expected " << contents_->size
               << " , but read " << blob.size() << " bytes.";
    *error = LockboxError::kSizeMismatch;
    return false;
  }

  // Append the salt to the data.
  brillo::Blob salty_blob(blob);
  salty_blob.insert(salty_blob.end(),
                    contents_->salt,
                    contents_->salt + contents_->salt_size);

  SecureBlob hash = CryptoLib::Sha256(salty_blob);
  // Maybe release the duplicate blob data.
  // TODO(wad) Add Crypto::GatherSha256 which takes a vector of blobs.
  salty_blob.resize(0);

  DCHECK(hash.size() == kReservedDigestBytes);
  // Validate the data hash versus the stored hash.
  if (brillo::SecureMemcmp(contents_->hash, hash.data(),
                             sizeof(contents_->hash))) {
    LOG(ERROR) << "Verify() hash mismatch!";
    *error = LockboxError::kHashMismatch;
    return false;
  }
  DLOG(INFO) << "Verify() verified "
             << blob.size() << " of " << contents_->size << " bytes.";
  return true;
}

bool Lockbox::Store(const brillo::Blob& blob, LockboxError* error) {
  unsigned int nvram_size;

  if (!TpmIsReady()) {
    LOG(ERROR) << "Store() called when TPM was not ready!";
    *error = LockboxError::kTpmError;
    return false;
  }

  // Ensure we have the space ready.
  if (!tpm_->IsNvramDefined(nvram_index_)) {
    LOG(ERROR) << "Store() called with no NVRAM space.";
    *error = LockboxError::kNoNvramSpace;
    return false;
  }
  if (tpm_->IsNvramLocked(nvram_index_)) {
    LOG(ERROR) << "Store() called with a locked NVRAM space.";
    *error = LockboxError::kNvramInvalid;
    return false;
  }
  // Check defined NVRAM size.
  nvram_size = tpm_->GetNvramSize(nvram_index_);
  switch (nvram_size) {
  case kReservedNvramBytesV1:
    contents_->salt_size = kReservedSaltBytesV1;
    nvram_version_ = kNvramVersion1;
    break;
  case kReservedNvramBytesV2:
    contents_->salt_size = kReservedSaltBytesV2;
    nvram_version_ = kNvramVersion2;
    break;
  default:
    LOG(ERROR) << "Store() found unexpected NVRAM size " << nvram_size << ".";
    *error = LockboxError::kNvramInvalid;
    return false;
  }

  // Grab a salt from the TPM.
  brillo::Blob salt(0);
  if (IsEncryptionSaltInLockbox()) {
    if (!tpm_->GetRandomDataBlob(contents_->salt_size, &salt)) {
      LOG(ERROR) << "Store() failed to get a salt from the TPM.";
      *error = LockboxError::kTpmError;
      return false;
    }
  } else {
    // Save a TPM command, and just fill the salt field with zeroes.
    LOG(INFO) << "Skipping random salt generation.";
    salt.resize(contents_->salt_size);
  }
  // Keep the data locally too.
  DCHECK(sizeof(contents_->salt) == kReservedSaltBytesV2);
  DCHECK(sizeof(contents_->salt) >= contents_->salt_size);
  memcpy(contents_->salt, salt.data(), contents_->salt_size);

  // Get the size of the data blob
  brillo::Blob size_blob;
  if (!GetSizeBlob(blob, &size_blob)) {
    LOG(ERROR) << "Store() data blob is too large.";
    *error = LockboxError::kTooLarge;
    return false;
  }
  contents_->size = blob.size();

  // Append the salt to the data and hash.
  brillo::Blob salty_blob(blob);
  salty_blob.insert(salty_blob.end(), salt.begin(), salt.end());

  // Insert the hash into the NVRAM.
  SecureBlob nvram_blob = CryptoLib::Sha256(salty_blob);
  DCHECK(kReservedDigestBytes == nvram_blob.size());
  memcpy(contents_->hash, nvram_blob.data(), sizeof(contents_->hash));

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

  // Write the hash to nvram
  if (!tpm_->WriteNvram(nvram_index_, SecureBlob(nvram_blob.begin(),
                        nvram_blob.end()))) {
    LOG(ERROR) << "Store() failed to write the attribute hash to NVRAM";
    *error = LockboxError::kTpmError;
    return false;
  }
  // Lock nvram index for writing.
  if (!tpm_->WriteLockNvram(nvram_index_)) {
    LOG(ERROR) << "Store() failed to lock the NVRAM space";
    *error = LockboxError::kTpmError;
    return false;
  }
  // Ensure the space is now locked.
  if (!tpm_->IsNvramLocked(nvram_index_)) {
    LOG(ERROR) << "NVRAM space did not lock as expected.";
    *error = LockboxError::kNvramFailedToLock;
    return false;
  }

  // Call out to mount-encrypted now that salt has been written.
  FinalizeMountEncrypted(nvram_version_ == 1 ? nvram_blob : salt);

  return true;
}

bool Lockbox::GetSizeBlob(const brillo::Blob& data,
                          brillo::Blob* size_bytes) const {
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

bool Lockbox::ParseSizeBlob(const brillo::Blob& blob, uint32_t* size) const {
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

// TODO(keescook) Write unittests for this.
void Lockbox::FinalizeMountEncrypted(const brillo::Blob &entropy) const {
  std::string hex;
  FilePath outfile_path;
  FILE *outfile;
  int rc;

  // Take hash of entropy and convert to hex string for cmdline.
  SecureBlob hash = CryptoLib::Sha256(entropy);
  hex = CryptoLib::BlobToHex(hash);

  process_->Reset(0);
  process_->AddArg(kMountEncrypted);
  process_->AddArg(kMountEncryptedFinalize);
  process_->AddArg(hex);

  // Redirect stdout/stderr somewhere useful for error reporting.
  outfile = platform_->CreateAndOpenTemporaryFile(&outfile_path);
  if (outfile) {
    process_->BindFd(fileno(outfile), STDOUT_FILENO);
    process_->BindFd(fileno(outfile), STDERR_FILENO);
  }

  rc = process_->Run();

  if (rc) {
    LOG(ERROR) << "Request to finalize encrypted mount failed ('"
               << kMountEncrypted << " "
               << kMountEncryptedFinalize << " "
               << hex << "', rc:" << rc << ")";
    if (outfile) {
      std::vector<std::string> output;
      std::vector<std::string>::iterator it;
      std::string contents;

      if (platform_->ReadFileToString(outfile_path, &contents)) {
        output = base::SplitString(contents, "\n", base::KEEP_WHITESPACE,
                                   base::SPLIT_WANT_ALL);
        for (it = output.begin(); it < output.end(); it++) {
          LOG(ERROR) << *it;
        }
      }
    }
  } else {
    LOG(INFO) << "Encrypted partition finalized.";
  }

  if (outfile)
    platform_->CloseFile(outfile);

  return;
}

}  // namespace cryptohome
