// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_POSTINST
#define CHROMEOS_POSTINST

#include <string>

bool RunPostInstall(const std::string& install_dir,
                    const std::string& install_dev);

#endif // CHROMEOS_POSTINST
