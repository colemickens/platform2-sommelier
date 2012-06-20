// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_postinst.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <vboot/CgptManager.h>

#include "chromeos_legacy.h"
#include "chromeos_install_config.h"
#include "chromeos_setimage.h"
#include "inst_util.h"

using std::string;

bool ConfigureInstall(
    const std::string& install_dev,
    const std::string& install_path,
    BiosType bios_type,
    InstallConfig* install_config) {

  Partition root = Partition(install_dev, install_path);

  string slot;
  switch (root.number()) {
    case 3:
      slot = "A";
      break;
    case 5:
      slot = "B";
      break;
    default:
      fprintf(stderr,
              "Not a valid target parition number: %i\n", root.number());
      return false;
  }

  string kernel_dev = MakePartitionDev(root.base_device(),
                                       root.number() - 1);

  string boot_dev = MakePartitionDev(root.base_device(),
                                     12);

  // Put the actual values on the result structure
  install_config->slot = slot;
  install_config->root = root;
  install_config->kernel = Partition(kernel_dev);
  install_config->boot = Partition(boot_dev);
  install_config->bios_type = bios_type;

  // If the bios type is known, there is no need to detect it, and we are done.
  if (bios_type != kBiosTypeUnknown)
    return true;

  return DetectBiosType(install_config);
}

bool DetectBiosType(InstallConfig* install_config) {
  // Look up the new kernel's command line.
  string kernel_cmd_line = DumpKernelConfig(install_config->kernel.device());
  return KernelConfigToBiosType(kernel_cmd_line, &install_config->bios_type);
}

bool KernelConfigToBiosType(const string& kernel_config, BiosType* type) {

  if (kernel_config.find("cros_secure") != string::npos) {
    *type = kBiosTypeSecure;
    return true;
  }

  if (kernel_config.find("cros_legacy") != string::npos) {
#ifdef __arm__
    // The Arm platform only uses U-Boot, but may set cros_legacy to mean
    // U-Boot without our secure boot modifications.
    *type = kBiosTypeUBoot;
#else
    *type = kBiosTypeLegacy;
#endif
    return true;
  }

  if (kernel_config.find("cros_efi") != string::npos) {
    *type = kBiosTypeEFI;
    return true;
  }

  // No recognized bios type was found
  printf("No recognized cros_XXX bios option on kernel command line\n");
  return false;
}

// Updates firmware. We must activate new firmware only after new kernel is
// actived (installed and made bootable), otherwise new firmware with all old
// kernels may lead to recovery screen (due to new key).
// TODO(hungte) Replace the shell execution by native code (crosbug.com/25407).
bool FirmwareUpdate(const string &install_dir, bool is_update) {
  int result;
  const char *mode;
  string command = install_dir + "/usr/sbin/chromeos-firmwareupdate";

  if (access(command.c_str(), X_OK) != 0) {
    printf("No firmware updates available.\n");
    return true;
  }

  if (is_update) {
    // Background auto update by Update Engine.
    mode = "autoupdate";
  } else {
    // Recovery image, or from command "chromeos-install".
    mode = "recovery";
  }

  command += " --mode=";
  command += mode;

  printf("Starting firmware updater (%s)\n", command.c_str());
  result = system(command.c_str());

  // Next step after postinst may take a lot of time (eg, disk wiping)
  // and people may confuse that as 'firmware update takes a long wait',
  // we explicitly prompt here.
  if (result == 0) {
    printf("Firmware update completed.\n");
  } else if (result == 3) {
    printf("Firmware can't be updated because booted from B (error code: %d)\n",
           result);
  } else {
    printf("Firmware update failed (error code: %d).\n", result);
  }

  return result == 0;
}

// Matches commandline arguments of chrome-chroot-postinst
//
// src_version of the form "10.2.3.4" or "12.3.2"
// install_dev of the form "/dev/sda3"
//
bool ChromeosChrootPostinst(const InstallConfig& install_config,
                            string src_version) {

  printf("ChromeosChrootPostinst(%s)\n",
         src_version.c_str());

  // Extract External ENVs
  bool is_factory_install = getenv("IS_FACTORY_INSTALL");
  bool is_recovery_install = getenv("IS_RECOVERY_INSTALL");
  bool is_install = getenv("IS_INSTALL");
  bool is_update = !is_factory_install && !is_recovery_install && !is_install;

  bool make_dev_readonly = false;

  if (is_update && VersionLess(src_version, "0.10.156.2")) {
    // See bug chromium-os:11517. This fixes an old FS corruption problem.
    printf("Patching new rootfs\n");
    if (!R10FileSystemPatch(install_config.root.device()))
      return false;
    make_dev_readonly=true;
  }

  // If this FS was mounted read-write, we can't do deltas from it. Mark the
  // FS as such
  Touch(install_config.root.mount() + "/.nodelta");  // Ignore Error on purpse

  printf("Set boot target to %s: Partition %d, Slot %s\n",
         install_config.root.device().c_str(),
         install_config.root.number(),
         install_config.slot.c_str());

  if (!SetImage(install_config)) {
    printf("SetImage failed.\n");
    return false;
  }

  printf("Syncing filesystems before changing boot order...\n");
  sync();

  printf("Updating Partition Table Attributes using CgptManager...\n");

  CgptManager cgpt_manager;

  int result = cgpt_manager.Initialize(install_config.root.base_device());
  if (result != kCgptSuccess) {
    printf("Unable to initialize CgptManager\n");
    return false;
  }

  result = cgpt_manager.SetHighestPriority(install_config.kernel.number());
  if (result != kCgptSuccess) {
    printf("Unable to set highest priority for kernel %d\n",
           install_config.kernel.number());
    return false;
  }

  // If it's not an update, pre-mark the first boot as successful
  // since we can't fall back on the old install.
  bool new_kern_successful = !is_update;
  result = cgpt_manager.SetSuccessful(install_config.kernel.number(),
                                      new_kern_successful);
  if (result != kCgptSuccess) {
    printf("Unable to set successful to %d for kernel %d\n",
           new_kern_successful,
           install_config.kernel.number());
    return false;
  }

  int numTries = 6;
  result = cgpt_manager.SetNumTriesLeft(install_config.kernel.number(),
                                        numTries);
  if (result != kCgptSuccess) {
    printf("Unable to set NumTriesLeft to %d for kernel %d\n",
           numTries,
           install_config.kernel.number());
    return false;
  }

  printf("Updated kernel %d with Successful = %d and NumTriesLeft = %d\n",
         install_config.kernel.number(), new_kern_successful, numTries);

  if (make_dev_readonly) {
    printf("Making dev %s read-only\n", install_config.root.device().c_str());
    MakeDeviceReadOnly(install_config.root.device());  // Ignore error
  }

  // At this point in the script, the new partition has been marked bootable
  // and a reboot will boot into it. Thus, it's important that any future
  // errors in this script do not cause this script to return failure unless
  // in factory mode.

  // We have a new image, making the ureadahead pack files
  // out-of-date.  Delete the files so that ureadahead will
  // regenerate them on the next reboot.
  // WARNING: This doesn't work with upgrade from USB, rather than full
  // install/recovery. We don't have support for it as it'll increase the
  // complexity here, and only developers do upgrade from USB.
  if (!RemovePackFiles("/var/lib/ureadahead")) {
    printf("RemovePackFiles Failed\n");
    if (is_factory_install)
      return false;
  }

  // Create a file indicating that the install is completed. The file
  // will be used in /sbin/chromeos_startup to run tasks on the next boot.
  // See comments above about removing ureadahead files.
  if (!Touch("/mnt/stateful_partition/.install_completed")) {
    printf("Touch(/mnt/stateful_partition/.install_completed) FAILED\n");
    if (is_factory_install)
      return false;
  }

  // In postinst in future, we may provide an option (ex, --update_firmware).
  string firmware_tag_file = (install_config.root.mount() +
                              "/root/.force_update_firmware");

  bool attempt_firmware_update = (!is_factory_install &&
                                  (access(firmware_tag_file.c_str(), 0) == 0));

  // In factory process, firmware is either pre-flashed or assigned by
  // mini-omaha server, and we don't want to try updates inside postinst.
  if (attempt_firmware_update) {
    if (!FirmwareUpdate(install_config.root.mount(), is_update)) {
      // Note: This will only rollback the ChromeOS verified boot target.
      // The assumption is that systems running firmware autoupdate
      // are not running legacy (non-ChromeOS) firmware. If the firmware
      // updater crashes or writes corrupt data rather than gracefully
      // failing, we'll probably need to recover with a recovery image.
      printf("Rolling back update due to failure installing required "
             "firmware.\n");

      // In all these checks below, we continue even if there's a failure
      // so as to cleanup as much as possible.
      new_kern_successful = false;
      bool rollback_successful = true;
      result = cgpt_manager.SetSuccessful(install_config.kernel.number(),
                                          new_kern_successful);
      if (result != kCgptSuccess) {
        rollback_successful = false;
        printf("Unable to set successful to %d for kernel %d\n",
               new_kern_successful,
               install_config.kernel.number());
      }

      numTries = 0;
      result = cgpt_manager.SetNumTriesLeft(install_config.kernel.number(),
                                            numTries);
      if (result != kCgptSuccess) {
        rollback_successful = false;
        printf("Unable to set NumTriesLeft to %d for kernel %d\n",
               numTries,
               install_config.kernel.number());
      }

      int priority = 0;
      result = cgpt_manager.SetPriority(install_config.kernel.number(),
                                        priority);
      if (result != kCgptSuccess) {
        rollback_successful = false;
        printf("Unable to set Priority to %d for kernel %d\n",
               priority,
               install_config.kernel.number());
      }

      if (rollback_successful)
        printf("Successfully updated GPT with all settings to rollback.\n");

      return false;
    }
  }

  printf("ChromeosChrootPostinst complete\n");
  return true;
}

// This program is called after an AutoUpdate or USB install. This script is
// a simple wrapper that performs the minimal setup necessary to run
// chromeos-chroot-postinst inside an install root chroot.

bool RunPostInstall(const string& install_dir,
                    const string& install_dev,
                    BiosType bios_type) {
  InstallConfig install_config;

  if (!ConfigureInstall(install_dir,
                        install_dev,
                        bios_type,
                        &install_config)) {
    printf("Configure failed.\n");
    return false;
  }

  string src_version;
  if (!LsbReleaseValue("/etc/lsb-release",
                       "CHROMEOS_RELEASE_VERSION",
                       &src_version) ||
      src_version.empty()) {
    printf("Failed to read /etc/lsb-release\n");
    return false;
  }

  // Log how we are configured.
  printf("PostInstall Configured: (%s, %s, %s, %s)\n",
         install_config.slot.c_str(),
         install_config.root.device().c_str(),
         install_config.kernel.device().c_str(),
         install_config.boot.device().c_str());

  if (!ChromeosChrootPostinst(install_config, src_version)) {
    printf("PostInstall Failed\n");
    return false;
  }

  printf("Syncing filesystem at end of postinst...\n");
  sync();

  // If we are installing to a ChromeOS Bios, we are done.
  if (install_config.bios_type == kBiosTypeSecure)
    return true;

  install_config.boot.set_mount("/tmp/boot_mnt");

  string cmd;

  cmd = StringPrintf("/bin/mkdir -p %s",
                     install_config.boot.mount().c_str());
  RUN_OR_RETURN_FALSE(cmd);

  cmd = StringPrintf("/bin/mount %s %s",
                     install_config.boot.device().c_str(),
                     install_config.boot.mount().c_str());
  RUN_OR_RETURN_FALSE(cmd);

  bool success = true;

  switch (install_config.bios_type)
  {
    case kBiosTypeUnknown:
    case kBiosTypeSecure:
      printf("Unexpected BiosType %d.\n", install_config.bios_type);
      success = false;
      break;

    case kBiosTypeUBoot:
      // The Arm platform only uses U-Boot, but may set cros_legacy to mean
      // U-Boot without secure boot modifications. This may need handling.
      if (!RunLegacyUBootPostInstall(install_config)) {
        printf("Legacy PostInstall failed.\n");
        success = false;
      }
      break;

    case kBiosTypeLegacy:
      if (!RunLegacyPostInstall(install_config)) {
        printf("Legacy PostInstall failed.\n");
        success = false;
      }
      break;

    case kBiosTypeEFI:
      if (!RunEfiPostInstall(install_config)) {
        printf("EFI PostInstall failed.\n");
        success = false;
      }
      break;
  }

  cmd = StringPrintf("/bin/umount %s",
                     install_config.boot.device().c_str());
  if (RunCommand(cmd.c_str()) != 0) {
    printf("Cmd: '%s' failed.\n", cmd.c_str());
    success = false;
  }

  return success;
}
