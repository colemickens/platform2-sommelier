// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/ntfs_mounter.h"

#include "cros-disks/platform.h"

using std::string;

namespace {

// Expected location of the ntfs-3g executable.
const char kMountProgramPath[] = "/usr/bin/ntfs-3g";

const char kMountUser[] = "ntfs-3g";

}  // namespace

namespace cros_disks {

const char NTFSMounter::kMounterType[] = "ntfs";

NTFSMounter::NTFSMounter(const string& source_path,
                         const string& target_path,
                         const string& filesystem_type,
                         const MountOptions& mount_options,
                         const Platform* platform)
    : FUSEMounter(source_path,
                  target_path,
                  filesystem_type,
                  mount_options,
                  platform,
                  kMountProgramPath,
                  kMountUser,
                  "",
                  {},
                  false) {}

}  // namespace cros_disks
