// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_BOOTLOCKBOX_NVRAM_BOOT_LOCKBOX_H_
#define CRYPTOHOME_BOOTLOCKBOX_NVRAM_BOOT_LOCKBOX_H_

#include <map>
#include <string>

#include <base/files/file_path.h>
#include <brillo/secure_blob.h>
#include <gtest/gtest_prod.h>

#include "cryptohome/bootlockbox/tpm_nvspace_interface.h"

#include "key_value_map.pb.h"  // NOLINT(build/include)

namespace cryptohome {

// A map that stores key-value pairs.
using KeyValueMap = std::map<std::string, std::string>;

const char kNVRamBootLockboxFilePath[] =
    "/var/lib/bootlockbox/nvram_boot_lockbox.pb";
// The max file file size for nvram_boot_lockbox.pb. Currently set
// to 1MB.
constexpr size_t kMaxFileSize = 1024 * 1024;
constexpr uint32_t kVersion = 1;

// NVRamBootLockbox is a key-value map that is stored on disk and its integrity
// is guaranteed by TPM NVRAM space. The key is usually an application defined
// string and the value is a SHA256 digest. The caller of NVRamBootLockbox is
// responsible for calculating the digest. NVRamBootLockbox is protected by the
// TPM and can only be updated before a user logs in after boot.
class NVRamBootLockbox {
 public:
  // Does not take ownership of |tpm_nvspace_utility|.
  explicit NVRamBootLockbox(TPMNVSpaceUtilityInterface* tpm_nvspace_utility);
  NVRamBootLockbox(TPMNVSpaceUtilityInterface* tpm_nvspace_utility,
                   const base::FilePath& bootlockbox_file_path);
  virtual ~NVRamBootLockbox();

  // Stores |digest| in bootlockbox.
  virtual bool Store(const std::string& key, const std::string& digest);

  // Reads digest identified by key.
  virtual bool Read(const std::string& key, std::string* digest);

  // Locks bootlockbox.
  virtual bool Finalize();

  // Gets BootLockbox state.
  virtual NVSpaceState GetState();

  // Defines NVRAM space.
  virtual bool DefineSpace();

  // Reads the key value map from disk and verifies the digest against the
  // digest stored in NVRAM space.
  virtual bool Load();

 protected:
  // Set BootLockbox state.
  virtual void SetState(const NVSpaceState state);

 private:
  // Writes to file, updates the digest in NVRAM space and updates local
  // local key_value_store_.
  bool FlushAndUpdate(const KeyValueMap& keyvals);

  // The file that stores serialized key_value_store_ on disk.
  base::FilePath boot_lockbox_filepath_;

  KeyValueMap key_value_store_;

  // The digest of the key value storage. The digest is stored in NVRAM
  // space and locked for writing after users logs in.
  std::string root_digest_;

  TPMNVSpaceUtilityInterface* tpm_nvspace_utility_;
  NVSpaceState nvspace_state_{NVSpaceState::kNVSpaceError};

  FRIEND_TEST(NVRamBootLockboxTest, LoadFailDigestMisMatch);
  FRIEND_TEST(NVRamBootLockboxTest, StoreLoadReadSuccess);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_BOOTLOCKBOX_NVRAM_BOOT_LOCKBOX_H_
