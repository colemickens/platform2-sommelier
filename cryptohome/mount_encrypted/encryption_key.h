// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_MOUNT_ENCRYPTED_ENCRYPTION_KEY_H_
#define CRYPTOHOME_MOUNT_ENCRYPTED_ENCRYPTION_KEY_H_

#include <stdint.h>

#include <string>
#include <vector>

#include <base/files/file_path.h>

#include <brillo/secure_blob.h>

#include "cryptohome/mount_encrypted.h"

// EncryptionKey takes care of the lifecycle of the encryption key protecting
// the encrypted stateful file system. This includes generation of the key,
// wrapping it using a system key which is stored in TPM NVRAM, as well as
// storing and loading the key to/from disk.
class EncryptionKey {
 public:
  explicit EncryptionKey(const base::FilePath& rootdir);

  // Loads the system key from TPM NVRAM.
  result_code SetTpmSystemKey();

  // Determines the system key to use in a production image on Chrome OS
  // hardware. Attempts to load the system key from TPM NVRAM or generates a new
  // system key. As a last resort, allows to continue without a system key to
  // cover systems where the NVRAM space is yet to be created by cryptohomed.
  result_code LoadChromeOSSystemKey();

  // While ChromeOS devices can store the system key in the NVRAM area, all the
  // rest will fallback through various places (kernel command line, BIOS UUID,
  // and finally a static value) for a system key.
  result_code SetInsecureFallbackSystemKey();

  // Loads the insecure well-known factory system key. This is used on factory
  // images instead of a proper key.
  result_code SetFactorySystemKey();

  // Initialize with a passed-in system key.
  result_code SetExternalSystemKey(const brillo::SecureBlob& system_key);

  // Load the encryption key from disk using the previously loaded system key.
  result_code LoadEncryptionKey();

  // Set encryption key to the passed-in value.
  void SetEncryptionKey(const brillo::SecureBlob& encryption_key);

  // Persist the key to disk and/or clean up. This involves making sure the
  // encryption key is written to disk so it can be recovered after reboot.
  void Persist();

  const brillo::SecureBlob& encryption_key() const { return encryption_key_; }
  bool is_fresh() const { return is_fresh_; }
  bool is_migration_allowed() const { return migration_allowed_; }
  bool did_finalize() const { return did_finalize_; }

  base::FilePath key_path() const { return key_path_; }
  base::FilePath needs_finalization_path() const {
    return needs_finalization_path_;
  }

 private:
  // Clean up disk state once the encryption key is properly wrapped by the
  // system key and persisted to disk.
  void Finalized();

  // Wrap the encryption key using the system key and write the result to disk.
  // Call Finalized() to clean up.
  bool DoFinalize();

  // Write the encryption key wrapped under an insecure, well-known wrapping key
  // to disk. This is needed for cases where the TPM cannot hold a secure system
  // key yet (e.g. due to the TPM NVRAM space being absent on TPM 1.2).
  void NeedsFinalization();

  // Paths.
  base::FilePath key_path_;
  base::FilePath needs_finalization_path_;

  // Whether we found a valid wrapped key file on disk on Load().
  bool valid_keyfile_ = false;

  // Whether the key is generated freshly, which happens if the system key is
  // missing or the key file on disk didn't exist, failed to decrypt, etc.
  bool is_fresh_ = false;

  // Whether it is OK to migrate an already existing unencrypted stateful
  // file system to a freshly created encrypted stateful file system. This is
  // only needed for devices that have been set up when Chrome OS didn't have
  // the stateful encryption feature yet.
  //
  // TODO(mnissler): Remove migration, it's no longer relevant.
  bool migration_allowed_ = false;

  // The system key is usually the key stored in TPM NVRAM that wraps the actual
  // encryption key. Empty if not available.
  brillo::SecureBlob system_key_;

  // The encryption key used for file system encryption.
  brillo::SecureBlob encryption_key_;

  // Whether finalization took place during Persist().
  bool did_finalize_ = false;
};

#endif  // CRYPTOHOME_MOUNT_ENCRYPTED_ENCRYPTION_KEY_H_
