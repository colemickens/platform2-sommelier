// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_MOUNT_ENCRYPTED_ENCRYPTED_FS_H_
#define CRYPTOHOME_MOUNT_ENCRYPTED_ENCRYPTED_FS_H_

#include <glib.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <brillo/secure_blob.h>

#include <cryptohome/mount_encrypted.h>

#define STATEFUL_MNT "mnt/stateful_partition"
#define ENCRYPTED_MNT STATEFUL_MNT "/encrypted"

namespace cryptohome {

enum class BindDir {
  BIND_SOURCE,
  BIND_DEST,
};

// Teardown stage: for granular teardowns
enum class TeardownStage {
  kTeardownUnbind,
  kTeardownDevmapper,
  kTeardownLoopDevice,
};

// BindMount represents a bind mount to be setup from
// source directories within the encrypted mount.
// EncryptedFs is responsible for setting up the bind mount
// once it sets up the encrypted mount.
struct BindMount {
  base::FilePath src;  // Location of bind source.
  base::FilePath dst;  // Destination of bind.
  std::string owner;
  std::string group;
  mode_t mode;
  bool submount;  // Submount is bound already.
};

// EncryptedFs sets up, tears down and cleans up encrypted
// stateful mounts. Given a root directory, the class
// sets up an encrypted mount at <root_dir>/ENCRYPTED_MOUNT.
class EncryptedFs {
 public:
  // Setup EncrytpedFs with the root directory.
  explicit EncryptedFs(const base::FilePath& mount_root);
  ~EncryptedFs() = default;

  // Setup mounts the encrypted mount by:
  // 1. Create a sparse file at <rootdir>/STATEFUL_MNT/encrypted.block
  // 2. Mounting a loop device on top of the sparse file.
  // 3. Mounting a dmcrypt device with the loop device as the backing
  //    device and the provided encryption key.
  // 4. Formatting the dmcrypt device as ext4 and mounting it at the
  //    mount_point.
  // If a sparse file already exists, Setup assumes that the stateful
  // mount has already been setup and attempts to mount the
  // | ext4 | dmcrypt | loopback | tower on top of the sparse file.
  // Parameters
  //   encryption_key - dmcrypt encryption key.
  //   rebuild - cleanup and recreate the encrypted mount.
  result_code Setup(const brillo::SecureBlob& encryption_key, bool rebuild);
  // Purge - obliterate the sparse file. This should be called only
  // when the encrypted fs is not mounted.
  int Purge(void);
  // Teardown - stepwise unmounts the | ext4 | dmcrypt | loopback | tower
  // on top of the sparse file.
  result_code Teardown(void);
  // CheckStates - Checks sanity for the stateful mount before mounting.
  result_code CheckStates(void);
  // ReportInfo - Reports the paths and bind mounts.
  result_code ReportInfo(void) const;
  // GetKey - Returns the key for the dmcrypt device. This is used
  // for finalization in devices that do not have the TPM available
  // initially while setting up the encrypted mount.
  brillo::SecureBlob GetKey() const;

 private:
  // CreateSparseBackingFile creates the sparse backing file for the
  // encrypted mount and returns an open fd, if successful.
  int CreateSparseBackingFile();
  // TeardownByStage allows higher granularity over teardown
  // processes.
  result_code TeardownByStage(TeardownStage stage, bool ignore_errors);

  // FilePaths used by the encrypted fs.
  base::FilePath rootdir_;
  base::FilePath stateful_mount_;
  base::FilePath block_path_;
  base::FilePath encrypted_mount_;
  std::string dmcrypt_name_;
  base::FilePath dmcrypt_dev_;
  std::vector<BindMount> bind_mounts_;
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_MOUNT_ENCRYPTED_ENCRYPTED_FS_H_
