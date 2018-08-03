// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Lockbox - class for storing tamper-evident data blobs.
#ifndef CRYPTOHOME_LOCKBOX_H_
#define CRYPTOHOME_LOCKBOX_H_

#include <memory>

#include <openssl/sha.h>

#include <base/macros.h>
#include <base/strings/string_util.h>
#include <brillo/process.h>
#include <brillo/secure_blob.h>

#include "cryptohome/platform.h"
#include "cryptohome/tpm.h"


namespace cryptohome {

struct LockboxContents;

enum class LockboxError {
  kNone = 0,
  kHashMismatch,               // Tampering or disk corruption.
  kSizeMismatch,               // Tampering or disk corruption.
  kInsufficientAuthorization,  // Bad call order or TPM state.
  kNoNvramSpace,               // No space is defined. (legacy install)
  kNoNvramData,                // Empty (unlocked) lockbox
  // All errors below indicate any number of system errors.
  kNvramInvalid,
  kNvramFailedToLock,
  kTpmNotReady,
  kTooLarge,
  kTpmError,  // Transient or unknown TPM error.
};

// Enable LockboxError to be used in LOG output.
std::ostream& operator<<(std::ostream& out, LockboxError error);

// Lockbox stores a blob of data in a tamper-evident manner.
//
// This class provides system integration for supporting tamper-evident
// storage using the TPM NVRAM locking and restricted TPM ownership to
// ensure that a data blob stored on disk has not been tampered with to
// the degree that can be cryptographically assured.
//
// Lockbox is not thread-safe and should not be accessed in parallel.
//
// A normal usage flow for Lockbox would be something as follows:
//
// Initializing new data against a lockbox (except with error checking :)
//   Lockbox lockbox(tpm, kNvramSpace);
//   LockboxError error;
//   lockbox->Destroy(&error);
//   lockbox->Create(&error);
//   lockbox->Store(mah_locked_data, &error);
//
// Verifying data can be done along these lines:
//   Lockbox lockbox(tpm, kNvramSpace);
//   lockbox->Load(&error);
//   lockbox->Verify(mah_maybe_tampered_with_dat, &error);
// ...
class Lockbox {
 public:
  // Populates the basic internal state of the Lockbox.
  //
  // Parameters
  // - tpm: a required pointer to a TPM object.
  // - nvram_index: the TPM NVRAM Index to use for this space.
  //
  // Lockbox requires a |tpm|.  If a NULL |tpm| is supplied, none of the
  // operations will succeed, but it should not crash or behave unexpectedly.
  // The |nvram_index| should be chosen carefully. See README.lockbox for info.
  Lockbox(Tpm* tpm, uint32_t nvram_index);
  virtual ~Lockbox();

  // Creates the backend state needed for this lockbox.
  //
  // Instantiates a new TPM NVRAM index lockable bWriteDefine to store
  // the hash and blob size of the data to lock away.
  //
  // Parameters
  // - error: will contain the LockboxError if false.
  // Returns
  // - true if a new space was instantiated or an old one could be used.
  // - false if the space cannot be created or claimed.
  virtual bool Create(LockboxError* error);

  // Destroys all backend state for this Lockbox.
  //
  // This call deletes the NVRAM space if defined.
  //
  // Parameters
  // - error: will contain the LockboxError if false.
  // Returns
  // - false if TPM Owner authorization is missing or the space cannot be
  //   destroyed.
  // - true if the space is already undefined or has been destroyed.
  virtual bool Destroy(LockboxError* error);

  // Loads the TPM NVRAM state date into memory
  //
  // Parameters
  // - error: LockboxError populated only on failure
  // Returns
  // - true if TPM NVRAM data is properly retrieved.
  // - false if the NVRAM data does not exist, is incorrect, or is unlocked.
  virtual bool Load(LockboxError* error);

  // Verifies the given blob against Load()d data.
  //
  // Parameters
  // - blob: data blob to verify.
  // - error: LockboxError populated only on failure
  // Returns
  // - true if verified
  // - false if the data could not be validated.
  virtual bool Verify(const brillo::Blob& blob, LockboxError* error);

  // Hashes, salts, sizes, and stores metadata required for verifying |data|
  // in TPM NVRAM for later verification.
  //
  // Parameters
  // - data: blob to store.
  // - error: LockboxError populated only on failure
  // Returns
  // - true if written to disk and NVRAM (if there is a tpm).
  // - false if the data could not be persisted.
  virtual bool Store(const brillo::Blob& data, LockboxError* error);

  // Replaces the tpm implementation.
  // Does NOT take ownership of the pointer.
  virtual void set_tpm(Tpm* tpm) { tpm_ = tpm; }

  // Replaces the process spawning implementation.
  // Does NOT take ownership of the pointer.
  virtual void set_process(brillo::Process* p) { process_ = p; }

  // Replaces the Platform class (only used for mount-encrypted).
  // Does NOT take ownership of the pointer.
  virtual void set_platform(Platform* p) { platform_ = p; }

  // Return NVRAM index.
  virtual uint32_t nvram_index() const { return nvram_index_; }
  // Return NVRAM version.
  virtual uint32_t nvram_version() const { return nvram_version_; }
  // Replaces the default NVRAM structure version.
  virtual void set_nvram_version(uint32_t version) { nvram_version_ = version; }

  virtual Tpm* tpm() { return tpm_; }

  // Provide a simple means to access the expected NVRAM contents.
  virtual const LockboxContents* contents() const { return contents_.get(); }

  // Tells if on this platform we store disk encryption salt in lockbox.
  // If true, it also requires additional protection for lockbox.
  // If false, the salt field is just filled with zeroes and not used.
  // Currently, the salt is stored separately for TPM 2.0.
  virtual bool IsEncryptionSaltInLockbox() const {
    return tpm_->GetVersion() != Tpm::TpmVersion::TPM_2_0;
  }

  // NVRAM structure versions.
  static const uint32_t kNvramVersion1;
  static const uint32_t kNvramVersion2;
  static const uint32_t kNvramVersionDefault;
  // Space reserved for the blob data size.
  static const uint32_t kReservedSizeBytes;
  static const uint32_t kReservedFlagsBytes;
  static const uint32_t kReservedSaltBytesV1;
  static const uint32_t kReservedSaltBytesV2;
  static const uint32_t kReservedDigestBytes;
  // The sum of the above sizes, accounting for possible salts.
  static const uint32_t kReservedNvramBytesV1;
  static const uint32_t kReservedNvramBytesV2;

  // Literals for running mount-encrypted helper.
  static const char * const kMountEncrypted;
  static const char * const kMountEncryptedFinalize;

 protected:
  // Returns true if we have the authorization needed to create/destroy
  // NVRAM spaces.
  virtual bool HasAuthorization() const;

  // Returns true if the tpm is owned and connected.
  virtual bool TpmIsReady() const;

  // Serialize the size of data to a Blob.
  virtual bool GetSizeBlob(const brillo::Blob& data,
                           brillo::Blob* size_bytes) const;

  // Parse a serialized size from the front of |blob|.
  virtual bool ParseSizeBlob(const brillo::Blob& blob, uint32_t* size) const;

  // Call out to the mount-encrypted helper to encrypt the key.
  virtual void FinalizeMountEncrypted(const brillo::Blob &entropy) const;

 private:
  Tpm* tpm_;
  uint32_t nvram_index_;
  uint32_t nvram_version_;
  std::unique_ptr<brillo::Process> default_process_;
  brillo::Process* process_;
  std::unique_ptr<LockboxContents> contents_;
  std::unique_ptr<Platform> default_platform_;
  Platform* platform_;

  DISALLOW_COPY_AND_ASSIGN(Lockbox);
};

// Defines the unmarshalled NVRAM contents.
#define CRYPTOHOME_LOCKBOX_SALT_LENGTH 32
struct LockboxContents {
  uint32_t size;
  uint8_t flags;
  uint8_t salt[CRYPTOHOME_LOCKBOX_SALT_LENGTH];
  uint8_t hash[SHA256_DIGEST_LENGTH];
  // Remaining variables are used internally and not stored to the NVRAM.
  uint32_t salt_size;
  bool loaded;
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_LOCKBOX_H_
