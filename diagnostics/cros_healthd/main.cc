// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <iostream>

#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>

#include "diagnostics/common/disk_utils.h"

int main(int argc, char** argv) {
  DEFINE_bool(probe_block_devices, false,
              "Exercise the ProbeNonRemovableBlockDeviceInfo routine");
  DEFINE_bool(probe_batteries, false, "Exercise the ProbeBatteryInfo routine");

  brillo::FlagHelper::Init(
      argc, argv, "cros_healthd - Device telemetry and diagnostics daemon.");

  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);

  if (FLAGS_probe_block_devices) {
    base::FilePath root_dir{"/"};
    auto devices =
        diagnostics::disk_utils::FetchNonRemovableBlockDevicesInfo(root_dir);
    VLOG(1) << "Found " << devices.size() << " non-removable block device(s)."
            << std::endl;
    std::cout << "path,size,type,manfid,name,serial" << std::endl;
    for (auto& device : devices) {
      std::cout << device->path << "," << std::dec << device->size << ","
                << device->type << ",0x" << std::hex
                << static_cast<int>(device->manfid) << "," << device->name
                << ",0x" << std::hex << device->serial << std::endl;
    }
  } else if (FLAGS_probe_batteries) {
    NOTIMPLEMENTED();
  } else {
    // TODO(pmoy): implement daemon
  }
  return EXIT_SUCCESS;
}
