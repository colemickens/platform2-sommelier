// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/files/file_path.h>
#include <base/logging.h>
#include <brillo/syslog_logging.h>

#include "oobe_config/load_oobe_config_usb.h"
#include "oobe_config/oobe_config.h"
#include "oobe_config/usb_utils.h"

using base::FilePath;

namespace oobe_config {

namespace {

void InitLog() {
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);
  logging::SetLogItems(true /* enable_process_id */,
                       true /* enable_thread_id */, true /* enable_timestamp */,
                       true /* enable_tickcount */);
}

}  // namespace

}  // namespace oobe_config

int main(int argc, char* argv[]) {
  oobe_config::InitLog();

  auto config_loader = oobe_config::LoadOobeConfigUsb::CreateInstance();
  if (!config_loader->Store()) {
    return 1;
  }

  // In both cases of success or failure remove the files from unencrypted
  // partition and ignore failures.
  config_loader->CleanupFilesOnDevice();

  return 0;
}
