// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "installer/chromeos_legacy.h"

#include <stdio.h>
#include <unistd.h>

#include <vector>

#include <base/files/file_enumerator.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>

#include "installer/inst_util.h"

using std::string;
using std::vector;

bool UpdateLegacyKernel(const InstallConfig& install_config) {
  string kernel_from =
      StringPrintf("%s/boot/vmlinuz", install_config.root.mount().c_str());

  string kernel_to = StringPrintf("%s/syslinux/vmlinuz.%s",
                                  install_config.boot.mount().c_str(),
                                  install_config.slot.c_str());

  return CopyFile(kernel_from, kernel_to);
}

string ExplandVerityArguments(const string& kernel_config,
                              const string& root_uuid) {
  string kernel_config_dm = ExtractKernelArg(kernel_config, "dm");

  // The verity config from the kernel contains short hand symbols for
  // partition names that we have to expand to specific UUIDs.

  // %U+1 -> XXX-YYY-ZZZ
  ReplaceAll(&kernel_config_dm, "%U+1", root_uuid);

  // PARTUUID=%U/PARTNROFF=1 -> PARTUUID=XXX-YYY-ZZZ
  ReplaceAll(&kernel_config_dm, "%U/PARTNROFF=1", root_uuid);

  return kernel_config_dm;
}

bool RunLegacyPostInstall(const InstallConfig& install_config) {
  printf("Running LegacyPostInstall\n");

  string cmd = StringPrintf("cp -nR '%s/boot/syslinux' '%s'",
                            install_config.root.mount().c_str(),
                            install_config.boot.mount().c_str());
  if (RunCommand(cmd.c_str()) != 0) {
    printf("Cmd: '%s' failed.\n", cmd.c_str());
    return false;
  }

  if (!UpdateLegacyKernel(install_config))
    return false;

  string kernel_config = DumpKernelConfig(install_config.kernel.device());
  string kernel_config_root = ExtractKernelArg(kernel_config, "root");

  // Prepare the new default.cfg

  string verity_enabled =
      (IsReadonly(kernel_config_root) ? "chromeos-vhd" : "chromeos-hd");

  string default_syslinux_cfg = StringPrintf(
      "DEFAULT %s.%s\n", verity_enabled.c_str(), install_config.slot.c_str());

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
                     install_config.root.device(), root_cfg_file))
    return false;

  string kernel_config_dm =
      ExplandVerityArguments(kernel_config, install_config.root.uuid());

  if (kernel_config_dm.empty()) {
    printf("Failed to extract Verity arguments.");
    return false;
  }

  // Insert the proper verity options for verity boots
  if (!ReplaceInFile(StringPrintf("DMTABLE%s", install_config.slot.c_str()),
                     kernel_config_dm, root_cfg_file))
    return false;

  return true;
}

// Copy a file from the root partition to the boot partition.
bool CopyBootFile(const InstallConfig& install_config,
                  const char* src,
                  const char* dst) {
  bool result = true;
  string src_path =
      StringPrintf("%s/%s", install_config.root.mount().c_str(), src);
  string dst_path =
      StringPrintf("%s/%s", install_config.boot.mount().c_str(), dst);

  // If the source file file exists, copy it into place, else do nothing.
  if (access(src_path.c_str(), R_OK) == 0) {
    printf("Copying '%s' to '%s'\n", src_path.c_str(), dst_path.c_str());
    result = CopyFile(src_path, dst_path);
  } else {
    printf("Not present to install: '%s'\n", src_path.c_str());
  }
  return result;
}

bool RunLegacyUBootPostInstall(const InstallConfig& install_config) {
  bool result = true;
  printf("Running LegacyUBootPostInstall\n");

  result &= CopyBootFile(
      install_config,
      StringPrintf("boot/boot-%s.scr.uimg", install_config.slot.c_str())
          .c_str(),
      "u-boot/boot.scr.uimg");
  result &= CopyBootFile(
      install_config,
      StringPrintf("boot/uEnv.%s.txt", install_config.slot.c_str()).c_str(),
      "uEnv.txt");
  result &= CopyBootFile(install_config, "boot/MLO", "MLO");
  result &= CopyBootFile(install_config, "boot/u-boot.img", "u-boot.img");

  return result;
}

bool UpdateEfiBootloaders(const InstallConfig& install_config) {
  bool result = true;
  const base::FilePath src_dir =
      base::FilePath(install_config.root.mount()).Append("boot/efi/boot");
  const base::FilePath dest_dir =
      base::FilePath(install_config.boot.mount()).Append("efi/boot");
  base::FileEnumerator file_enum(src_dir, false, base::FileEnumerator::FILES,
                                 "*.efi");
  for (auto src = file_enum.Next(); !src.empty(); src = file_enum.Next()) {
    const base::FilePath dest = dest_dir.Append(src.BaseName());
    if (!base::CopyFile(src, dest))
      result = false;
  }
  return result;
}

bool RunEfiPostInstall(const InstallConfig& install_config) {
  printf("Running EfiPostInstall\n");

  // Update the kernel we are about to use.
  if (!UpdateLegacyKernel(install_config))
    return false;

  if (!UpdateEfiBootloaders(install_config))
    return false;

  // Of the form: PARTUUID=XXX-YYY-ZZZ
  string kernel_config = DumpKernelConfig(install_config.kernel.device());
  string root_uuid = install_config.root.uuid();
  string kernel_config_dm = ExplandVerityArguments(kernel_config, root_uuid);

  string grub_filename =
      StringPrintf("%s/efi/boot/grub.cfg", install_config.boot.mount().c_str());

  // Read in the grub.cfg to be updated.
  string grub_src;
  if (!ReadFileToString(grub_filename, &grub_src)) {
    printf("Unable to read grub template file %s\n", grub_filename.c_str());
    return false;
  }

  string output;
  if (!EfiGrubUpdate(grub_src, install_config.slot, root_uuid, kernel_config_dm,
                     &output)) {
    return false;
  }

  // Write out the new grub.cfg.
  if (!WriteStringToFile(output, grub_filename)) {
    printf("Unable to write boot menu file %s\n", grub_filename.c_str());
    return false;
  }

  // We finished.
  return true;
}

bool EfiGrubUpdate(const string& input,
                   const string& slot,
                   const string& root_uuid,
                   const string& verity_args,
                   string* output) {
  // Split the file contents into lines.
  vector<string> file_lines;
  SplitString(input, '\n', &file_lines);

  // Search pattern for lines are related to our slot.
  string kernel_pattern = StringPrintf("/syslinux/vmlinuz.%s", slot.c_str());

  for (vector<string>::iterator line = file_lines.begin();
       line < file_lines.end(); line++) {
    if (line->find(kernel_pattern) != string::npos) {
      if (ExtractKernelArg(*line, "dm").empty()) {
        // If it's an unverified boot line, just set the root partition to boot.
        if (!SetKernelArg("root",
                          StringPrintf("PARTUUID=%s", root_uuid.c_str()),
                          &(*line))) {
          printf("Unable to update unverified root flag in %s.\n",
                 line->c_str());
          return false;
        }
      } else {
        if (!SetKernelArg("dm", verity_args, &(*line))) {
          printf("Unable to update verified dm flag.\n");
          return false;
        }
      }
    }
  }

  // Join the lines back into file contents.
  JoinStrings(file_lines, "\n", output);

  // Other EFI post-install actions, if any, go here.
  return true;
}
