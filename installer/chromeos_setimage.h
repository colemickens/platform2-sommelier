// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SETIMAGE
#define CHROMEOS_SETIMAGE

#include <string>

bool SetImage(const std::string& install_dir,    // /tmp/mnt_xxx
              const std::string& root_dev,       // /dev/sda
              const std::string& rootfs_image,   // /dev/sda5
              const std::string& kernel_image);  // /dev/sda4

#endif // CHROMEOS_SETIMAGE
