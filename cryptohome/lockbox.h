// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Lockbox - class for storing tamper-evident data blobs.
#ifndef CRYPTOHOME_LOCKBOX_H_
#define CRYPTOHOME_LOCKBOX_H_

#include <openssl/sha.h>

#include <base/basictypes.h>
#include <base/memory/scoped_ptr.h>
#include <base/string_util.h>
#include <chromeos/utility.h>

#include "crypto.h"
#include "tpm.h"


namespace cryptohome {

struct LockboxContents;

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
//   Lockbox::ErrorId error;
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
  typedef enum {
    kErrorIdNone = 0,
    kErrorIdHashMismatch,  // Tampering or disk corruption.
    kErrorIdSizeMismatch,  // Tampering or disk corruption.
    kErrorIdInsufficientAuthorization,  // Bad call order or TPM state.
    kErrorIdNoNvramSpace,  // No space is defined. (legacy install)
    kErrorIdNoNvramData,  // Empty (unlocked) lockbox
    // All errors below indicate any number of system errors.
    kErrorIdNvramInvalid,
    kErrorIdNvramFailedToLock,
    kErrorIdTpmNotReady,
    kErrorIdTooLarge,
    kErrorIdTpmError,  // Transient or unknown TPM error.
  } ErrorId;

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
  // - error: will contain the ErrorId if false.
  // Returns
  // - true if a new space was instantiated or an old one could be used.
  // - false if the space cannot be created or claimed.
  virtual bool Create(ErrorId* error);

  // Destroys all backend state for this Lockbox.
  //
  // This call deletes the NVRAM space if defined.
  //
  // Parameters
  // - error: will contain the ErrorId if false.
  // Returns
  // - false if TPM Owner authorization is missing or the space cannot be
  //   destroyed.
  // - true if the space is already undefined or has been destroyed.
  virtual bool Destroy(ErrorId* error);

  // Loads the TPM NVRAM state date into memory
  //
  // Parameters
  // - error: ErrorId populated only on failure
  // Returns
  // - true if TPM NVRAM data is properly retrieved.
  // - false if the NVRAM data does not exist, is incorrect, or is unlocked.
  virtual bool Load(ErrorId* error);

  // Verifies the given blob against Load()d data.
  //
  // Parameters
  // - blob: data blob to verify.
  // - error: ErrorId populated only on failure
  // Returns
  // - true if verified
  // - false if the data could not be validated.
  virtual bool Verify(const chromeos::Blob& blob, ErrorId* error);

  // Hashes, salts, sizes, and stores metadata required for verifying |data|
  // in TPM NVRAM for later verification.
  //
  // Parameters
  // - data: blob to store.
  // - error: ErrorId populated only on failure
  // Returns
  // - true if written to disk and NVRAM (if there is a tpm).
  // - false if the data could not be persisted.
  virtual bool Store(const chromeos::Blob& data, ErrorId* error);

  // Replaces the crypto implementation.
  // Does NOT take ownership of the pointer.
  virtual void set_crypto(Crypto* crypto) { crypto_ = crypto; }

  virtual Crypto* crypto() { return crypto_; }

  // Replaces the tpm implementation.
  // Does NOT take ownership of the pointer.
  virtual void set_tpm(Tpm* tpm) { tpm_ = tpm; }

  virtual Tpm* tpm() { return tpm_; }

  // Provide a simple means to access the expected NVRAM contents.
  virtual const LockboxContents* contents() const { return contents_.get(); }

  // Space reserved for the blob data size.
  static const uint32_t kReservedSizeBytes;
  static const uint32_t kReservedFlagsBytes;
  static const uint32_t kReservedSaltBytes;
  static const uint32_t kReservedDigestBytes;
  // The sum of the above sizes.
  static const uint32_t kReservedNvramBytes;

 protected:
  // Returns true if we have the authorization needed to create/destroy
  // NVRAM spaces.
  virtual bool HasAuthorization() const;

  // Returns true if the tpm is owned and connected.
  virtual bool TpmIsReady() const;

  // Serialize the size of data to a Blob.
  virtual bool GetSizeBlob(const chromeos::Blob& data,
                           chromeos::Blob* size_bytes) const;

  // Parse a serialized size from the front of |blob|.
  virtual bool ParseSizeBlob(const chromeos::Blob& blob, uint32_t* size) const;

 private:
  Tpm* tpm_;
  uint32_t nvram_index_;
  scoped_ptr<Crypto> default_crypto_;
  Crypto* crypto_;
  scoped_ptr<LockboxContents> contents_;

  DISALLOW_COPY_AND_ASSIGN(Lockbox);
};

// Defines the unmarshalled NVRAM contents.
#define CRYPTOHOME_LOCKBOX_SALT_LENGTH 7
struct LockboxContents {
  uint32_t size;
  uint8_t flags;
  uint8_t salt[CRYPTOHOME_LOCKBOX_SALT_LENGTH];
  uint8_t hash[SHA256_DIGEST_LENGTH];
  bool loaded;
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_LOCKBOX_H_
