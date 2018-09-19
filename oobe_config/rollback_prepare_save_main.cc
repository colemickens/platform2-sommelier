// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/files/file_path.h>
#include <brillo/syslog_logging.h>

#include "oobe_config/rollback_helper.h"

namespace {

void InitLog() {
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);
  logging::SetLogItems(true /* enable_process_id */,
                       true /* enable_thread_id */, true /* enable_timestamp */,
                       true /* enable_tickcount */);
}

}  // namespace

int main(int argc, char* argv[]) {
  InitLog();
  if (oobe_config::PrepareSave(base::FilePath(),
                               false /* ignore_permissions_for_testing */))
    return 0;
  return 1;
}
