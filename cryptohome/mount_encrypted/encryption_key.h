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

  // Set encryption key to the passed-in value and persist it to disk. Requires
  // a usable system key to be present.
  void PersistEncryptionKey(const brillo::SecureBlob& encryption_key);

  const brillo::SecureBlob& encryption_key() const { return encryption_key_; }
  bool is_fresh() const { return is_fresh_; }
  bool is_migration_allowed() const { return migration_allowed_; }
  bool did_finalize() const { return did_finalize_; }

  base::FilePath key_path() const { return key_path_; }
  base::FilePath needs_finalization_path() const {
    return needs_finalization_path_;
  }

 private:
  // Encrypts the |encryption_key_| under |system_key_| and writes the result to
  // disk to the |key_path_| file.
  void Finalize();

  // Paths.
  base::FilePath key_path_;
  base::FilePath needs_finalization_path_;

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
