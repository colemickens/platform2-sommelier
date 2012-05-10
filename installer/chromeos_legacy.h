// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LEGACY_H_
#define CHROMEOS_LEGACY_H_

#include "chromeos_install_config.h"

// Attempts to update boot files needed by the legacy bios boot
// (syslinux config files) on the boot partition. Returns false on error.
bool RunLegacyPostInstall(const InstallConfig& install_config);

// Attempts to update boot files needed by u-boot (not our secure u-boot)
// in some development situations.
bool RunLegacyUBootPostInstall(const InstallConfig& install_config);

// Attempts to update boot files needed by the EFI bios boot
// (grub config files) on the boot partition. Returns false on error.
bool RunEfiPostInstall(const InstallConfig& install_config);

#endif  // CHROMEOS_LEGACY_H_
