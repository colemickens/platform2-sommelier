// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_legacy.h"

#include <stdio.h>
#include <unistd.h>

#include "inst_util.h"

using std::string;

bool RunLegacyPostInstall(const InstallConfig& install_config) {
  printf("Running LegacyPostInstall\n");

  string cmd = StringPrintf("cp -nR '%s/boot/syslinux' '%s'",
                            install_config.root.mount().c_str(),
                            install_config.boot.mount().c_str());
  if (RunCommand(cmd.c_str()) != 0) {
    printf("Cmd: '%s' failed.\n", cmd.c_str());
    return false;
  }

  string kernel_from = StringPrintf("%s/boot/vmlinuz",
                                    install_config.root.mount().c_str());

  string kernel_to = StringPrintf("%s/syslinux/vmlinuz.%s",
                                  install_config.boot.mount().c_str(),
                                  install_config.slot.c_str());

  if (!CopyFile(kernel_from, kernel_to))
    return false;

  string kernel_config = DumpKernelConfig(install_config.kernel.device());
  string kernel_config_root = ExtractKernelArg(kernel_config, "root");
  string kernel_config_dm = ExtractKernelArg(kernel_config, "dm");

  // Of the form: PARTUUID=XXX-YYY-ZZZ
  string root_uuid = StringPrintf("PARTUUID=%s",
                                  install_config.root.uuid().c_str());

  // The verity config from the kernel contains short hand symbols for
  // partition names that we can't get away with.

  // payload=%U+1 -> payload=PARTUUID=XXX-YYY-ZZZ
  if (!SetKernelArg("payload", root_uuid, &kernel_config_dm)) {
    printf("Failed to find 'payload=' in kernel verity config.");
    return false;
  }

  // hashtree=%U+1 -> hashtree=PARTUUID=XXX-YYY-ZZZ
  if (!SetKernelArg("hashtree", root_uuid, &kernel_config_dm)) {
    printf("Failed to find 'hashtree=' in kernel verity config.");
    return false;
  }

  // Prepare the new default.cfg

  string verity_enabled = ((kernel_config_root == "/dev/dm-0") ?
                           "chromeos-vhd" : "chromeos-hd");

  string default_syslinux_cfg = StringPrintf("DEFAULT %s.%s\n",
                                             verity_enabled.c_str(),
                                             install_config.slot.c_str());

  if (!WriteStringToFile(default_syslinux_cfg,
                         StringPrintf("%s/syslinux/default.cfg",
                                      install_config.boot.mount().c_str())))
    return false;

  // Prepare the new root.A/B.cfg

  string root_cfg_file = StringPrintf("%s/syslinux/root.%s.cfg",
                                      install_config.boot.mount().c_str(),
                                      install_config.slot.c_str());

  // Copy over the unmodified version for this release...
  if (!CopyFile(StringPrintf("%s/boot/syslinux/root.%s.cfg",
                             install_config.root.mount().c_str(),
                             install_config.slot.c_str()),
                root_cfg_file))
    return false;

  // Insert the proper root device for non-verity boots
  if (!ReplaceInFile(StringPrintf("HDROOT%s", install_config.slot.c_str()),
                     install_config.root.device(),
                     root_cfg_file))
    return false;

  // Insert the proper verity options for verity boots
  if (!ReplaceInFile(StringPrintf("DMTABLE%s", install_config.slot.c_str()),
                     kernel_config_dm,
                     root_cfg_file))
    return false;

  return true;
}

bool RunLegacyUBootPostInstall(const InstallConfig& install_config) {
  printf("Running LegacyUBootPostInstall\n");

  string src_img = StringPrintf("%s/boot/boot-%s.scr.uimg",
                                install_config.root.mount().c_str(),
                                install_config.slot.c_str());

  string dst_img = StringPrintf("%s/u-boot/boot.scr.uimg",
                                install_config.boot.mount().c_str());

  // If the source img file exists, copy it into place, else do
  // nothing.
  if (access(src_img.c_str(), R_OK) == 0) {
    printf("Copying '%s' to '%s'\n", src_img.c_str(), dst_img.c_str());
    return CopyFile(src_img, dst_img);
  } else {
    printf("Not present to install: '%s'\n", src_img.c_str());
    return true;
  }
}

bool RunEfiPostInstall(const InstallConfig& install_config) {
  printf("Running EfiPostInstall\n");
  // TODO(dgarret) Support EFI by fixing GRUB configuration files.
  printf("EFI BIOS is temporarily not supported.\n"
         "You probably need to manually edit the GRUB configuration files "
         "after installation.\n");
  // 'true' is required here to prevent blocking factory installation for most
  // partner devices, until EFI is officially supported.
  return true;
}
