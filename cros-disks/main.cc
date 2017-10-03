// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>

#include "cros-disks/daemon.h"

int main(int argc, char** argv) {
  DEFINE_bool(foreground, false, "Run in foreground");
  DEFINE_bool(no_session_manager, false,
              "run without the expectation of a session manager.");
  DEFINE_int32(log_level, 0,
               "Logging level - 0: LOG(INFO), 1: LOG(WARNING), 2: LOG(ERROR), "
               "-1: VLOG(1), -2: VLOG(2), ...");
  brillo::FlagHelper::Init(argc, argv, "Chromium OS Disk Daemon");

  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);
  logging::SetMinLogLevel(FLAGS_log_level);

  if (!FLAGS_foreground)
    PLOG_IF(FATAL, ::daemon(0, 0) == 1) << "Failed to create daemon";

  bool has_session_manager = !FLAGS_no_session_manager;
  cros_disks::Daemon daemon(has_session_manager);
  return daemon.Run();
}
