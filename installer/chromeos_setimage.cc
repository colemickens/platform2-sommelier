// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_setimage.h"
#include "chromeos_verity.h"

#include <stdio.h>
#include <stdlib.h>

#include "inst_util.h"

using std::string;

//
// There is nothing in our codebase that calls chromeos-setimage,
// except for post install, so only it's use case is required
// here. I think.
//

bool SetImage(const string& install_dir,
              const string& root_dev,
              const string& rootfs_dev,
              const string& kernel_dev) {

  printf("SetImage(%s, %s, %s, %s)\n",
         install_dir.c_str(),
         root_dev.c_str(),
         rootfs_dev.c_str(),
         kernel_dev.c_str());

  // Re-hash the root filesystem and use the table for dm-verity.
  // We extract the parameters for verification from the kernel
  // partition, but we regenerate and reappend the hash tree to
  // keep the updater from needing to manage them explicitly.
  // Instead, rootfs integrity will be validated on next boot through
  // the verified kernel configuration.

  string kernel_config = DumpKernelConfig(kernel_dev);

  printf("KERNEL_CONFIG: %s\n", kernel_config.c_str());

  // An example value: <root_hexdigest and salt values shortened>
  //
  // quiet loglevel=1 console=tty2 init=/sbin/init add_efi_memmap boot=local
  // noresume noswap i915.modeset=1 cros_secure tpm_tis.force=1
  // tpm_tis.interrupts=0 nmi_watchdog=panic,lapic root=/dev/dm-0 rootwait
  // ro dm_verity.error_behavior=3 dm_verity.max_bios=-1 dm_verity.dev_wait=1
  // dm="vroot none ro,0 1740800 verity payload=%U+1 hashtree=%U+1
  // hashstart=1740800 alg=sha1 root_hexdigest=30348c07f salt=a9864eaf11f4
  // 66fc48dffef" noinitrd cros_debug vt.global_cursor_default=0 kern_guid=%U
  //

  string kernel_config_root = ExtractKernelArg(kernel_config, "root");
  string dm_config = ExtractKernelArg(kernel_config, "dm");
  std::vector<string> dm_parts;
  SplitString(dm_config, ',', &dm_parts);

  if (dm_parts.size() != 2) {
    printf("Unexpected dm configuration for kernel '%s'\n", dm_config.c_str());
    return false;
  }

  // Extract dm specific options
  string verity_preamble = dm_parts[0];
  string dm_kernel_cfg = dm_parts[1];

  // Extract specific verity arguments
  string rootfs_sectors = ExtractKernelArg(dm_kernel_cfg, "hashstart");
  string verity_algorithm = ExtractKernelArg(dm_kernel_cfg, "alg");
  string expected_hash = ExtractKernelArg(dm_kernel_cfg,
                                               "root_hexdigest");
  string salt = ExtractKernelArg(dm_kernel_cfg, "salt");

  bool enable_rootfs_verification = (kernel_config_root == "/dev/dm-0");

  if (!enable_rootfs_verification)
    MakeFileSystemRw(rootfs_dev, true);

  if (chromeos_verity(verity_algorithm.c_str(), rootfs_dev.c_str(),
                      getpagesize(),
                      (uint64_t)(atoi(rootfs_sectors.c_str()) / 8),
                      salt.c_str(), expected_hash.c_str(),
                      enable_rootfs_verification) != 0)
    return false;

  return true;
}
