// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>
#include <brillo/syslog_logging.h>

#include "oobe_config/oobe_config.h"

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

  // TODO(zentaro): Replace with real implementation.
  LOG(INFO) << oobe_config::Hello() << " restore.";
}
