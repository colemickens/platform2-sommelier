// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_POSTINST_H_
#define CHROMEOS_POSTINST_H_

#include "chromeos_install_config.h"

#include <string>

bool ConfigureInstall(
    const std::string& install_dev,
    const std::string& install_path,
    InstallConfig* install_config);

bool RunPostInstall(const std::string& install_dir,
                    const std::string& install_dev);

#endif // CHROMEOS_POSTINST_H_
