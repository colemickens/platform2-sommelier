// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/command_line.h>
#include <chromeos/syslog_logging.h>

#include "germ/daemon.h"

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  chromeos::InitLog(chromeos::kLogToSyslog);

  germ::Daemon daemon;
  return daemon.Run();
}
