// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_POSTINST_H_
#define CHROMEOS_POSTINST_H_

#include "chromeos_install_config.h"

#include <string>

// Create the configuration structure used during an install.
bool ConfigureInstall(
    const std::string& install_dev,
    const std::string& install_path,
    BiosType bios_type,
    InstallConfig* install_config);

// Find the new kernel's command line and use it to find the
// current bios type.
bool DetectBiosType(InstallConfig* install_config);

// Find out bios type in use from a kernel command line.
bool KernelConfigToBiosType(const std::string& kernel_config, BiosType* type);

// Perform the post install operation. This is used after a kernel and
// rootfs have been copied into to place to make the valid and set them
// up for the next boot.
bool RunPostInstall(const std::string& install_dir,
                    const std::string& install_dev,
                    BiosType bios_type);

#endif // CHROMEOS_POSTINST_H_
