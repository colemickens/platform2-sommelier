// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/command_line.h>
#include <base/logging.h>
#include <brillo/syslog_logging.h>

#include "cups_proxy/daemon.h"

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderr);
  printing::Daemon daemon;

  return daemon.Run();
}
