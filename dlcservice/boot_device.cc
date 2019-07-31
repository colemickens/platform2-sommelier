// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dlcservice/boot_device.h"

#include <linux/limits.h>

#include <base/files/file_util.h>
#include <base/strings/string_util.h>
#include <rootdev/rootdev.h>

using base::FilePath;
using std::string;

namespace dlcservice {

bool BootDevice::IsRemovableDevice(const string& device) {
  string sysfs_block = SysfsBlockDevice(device);
  string removable;
  if (sysfs_block.empty() ||
      !base::ReadFileToString(FilePath(sysfs_block).Append("removable"),
                              &removable)) {
    return false;
  }
  base::TrimWhitespaceASCII(removable, base::TRIM_ALL, &removable);
  return removable == "1";
}

string BootDevice::GetBootDevice() {
  char boot_path[PATH_MAX];
  // Resolve the boot device path fully, including dereferencing through
  // dm-verity.
  int ret = rootdev(boot_path, sizeof(boot_path), true, /*full resolution*/
                    false /*do not remove partition #*/);
  if (ret < 0) {
    LOG(ERROR) << "rootdev failed to find the root device";
    return "";
  }
  LOG_IF(WARNING, ret > 0) << "rootdev found a device name with no device node";

  // This local variable is used to construct the return string and is not
  // passed around after use.
  return boot_path;
}

string BootDevice::SysfsBlockDevice(const string& device) {
  FilePath device_path(device);
  if (device_path.DirName().value() != "/dev") {
    return "";
  }
  return FilePath("/sys/block").Append(device_path.BaseName()).value();
}

}  // namespace dlcservice
