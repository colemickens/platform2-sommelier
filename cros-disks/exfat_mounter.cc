// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/exfat_mounter.h"

#include "cros-disks/platform.h"

using std::string;

namespace {

// Expected location of the exfat-fuse executable.
const char kMountProgramPath[] = "/usr/sbin/mount.exfat-fuse";

const char kMountUser[] = "fuse-exfat";

}  // namespace

namespace cros_disks {

const char ExFATMounter::kMounterType[] = "exfat";

ExFATMounter::ExFATMounter(const string& source_path,
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
                  false,
                  true /* unprivileged_mount */) {}

}  // namespace cros_disks
