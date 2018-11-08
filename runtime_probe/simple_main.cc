/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <brillo/syslog_logging.h>

#include "runtime_probe/daemon.h"

int main(int argc, char* argv[]) {
  // TODO(itspeter): b/119939954, Support command line arguments.
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderr);

  LOG(INFO) << "Starting Runtime Probe";
  runtime_probe::Daemon daemon;
  daemon.Run();

  return 0;
}
