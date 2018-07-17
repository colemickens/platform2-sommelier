// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INSTALLER_CHROMEOS_LEGACY_H_
#define INSTALLER_CHROMEOS_LEGACY_H_

#include <string>

#include "installer/chromeos_install_config.h"

// Attempts to update boot files needed by the legacy bios boot
// (syslinux config files) on the boot partition. Returns false on error.
bool RunLegacyPostInstall(const InstallConfig& install_config);

// Attempts to update boot files needed by u-boot (not our secure u-boot)
// in some development situations.
bool RunLegacyUBootPostInstall(const InstallConfig& install_config);

// Attempts to update boot files needed by the EFI bios boot
// (grub config files) on the boot partition. Returns false on error.
bool RunEfiPostInstall(const InstallConfig& install_config);

// Exported to make it testable.
bool EfiGrubUpdate(const std::string& input,
                   const std::string& slot,
                   const std::string& root_uuid,
                   const std::string& verity_args,
                   std::string* output);

#endif  // INSTALLER_CHROMEOS_LEGACY_H_
