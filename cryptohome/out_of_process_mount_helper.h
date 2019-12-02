// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// OutOfProcessMountHelper objects carry out mount(2) and unmount(2) operations
// for a single cryptohome mount, but do so out-of-process.

#ifndef CRYPTOHOME_OUT_OF_PROCESS_MOUNT_HELPER_H_
#define CRYPTOHOME_OUT_OF_PROCESS_MOUNT_HELPER_H_

#include <sys/types.h>

#include <memory>
#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/macros.h>
#include <brillo/process.h>
#include <brillo/secure_blob.h>
#include <chromeos/dbus/service_constants.h>

#include "cryptohome/mount_constants.h"
#include "cryptohome/mount_helper.h"
#include "cryptohome/platform.h"

using base::FilePath;

namespace cryptohome {

class OutOfProcessMountHelper : public EphemeralMountHelperInterface {
 public:
  OutOfProcessMountHelper(const brillo::SecureBlob& system_salt,
                          bool legacy_home,
                          Platform* platform)
      : system_salt_(system_salt),
        legacy_home_(legacy_home),
        platform_(platform),
        username_(),
        write_to_helper_(-1) {}
  ~OutOfProcessMountHelper() = default;

  // Carries out dircrypto mount(2) operations for an ephemeral cryptohome,
  // but does so out of process.
  bool PerformEphemeralMount(const std::string& username) override;

  // Tears down an ephemeral cryptohome mount by terminating the out-of-process
  // helper.
  void TearDownEphemeralMount() override;

  // Returns whether an ephemeral mount operation can be performed.
  bool CanPerformEphemeralMount() const override;

  // Returns whether a mount operation has been performed.
  bool MountPerformed() const override;

  // Returns whether |path| is the destination of an existing mount.
  bool IsPathMounted(const base::FilePath& path) const override;

 private:
  // Kills the out-of-process helper if it's still running, and Reset()s the
  // Process instance to close all pipe file descriptors.
  void KillOutOfProcessHelperIfNecessary();

  // Stores the global system salt.
  brillo::SecureBlob system_salt_;

  // Whether to make the legacy home directory (/home/chronos/user) available.
  bool legacy_home_;

  Platform* platform_;  // Un-owned.

  // Username the mount belongs to, if a mount has been performed.
  // Empty otherwise.
  std::string username_;

  // Tracks the helper process.
  std::unique_ptr<brillo::Process> helper_process_;

  // Pipe used to communicate with the helper process.
  // This file descriptor is owned by |helper_process_|, so it's not
  // scoped.
  int write_to_helper_;

  DISALLOW_COPY_AND_ASSIGN(OutOfProcessMountHelper);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_OUT_OF_PROCESS_MOUNT_HELPER_H_
